#include "tcp_server.h"
#include "buf_pool.h"
#include "io_buf.h"
#include "reactor_buf.h"
#include "tcp_conn.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <strings.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include "message.h"

#define debug

// 链接资源管理
// 全部已经在线的链接信息
tcp_conn **tcp_server::conns = nullptr;

// 最大容量链接个数
int tcp_server::_max_conns = 0;

// 当前链接刻度
int tcp_server::_curr_conns = 0;

// 保护_curr_conns刻度修改的锁
pthread_mutex_t tcp_server::_conns_mutex = PTHREAD_MUTEX_INITIALIZER;

// 新增一个新建的连接
void tcp_server::increase_conn(int connfd, tcp_conn *conn) {
  pthread_mutex_lock(&_conns_mutex);
  conns[connfd] = nullptr;
  _curr_conns++;
  pthread_mutex_unlock(&_conns_mutex);
}

// 减少一个断开的连接
void tcp_server::decrease_conn(int connfd) {
  pthread_mutex_lock(&_conns_mutex);
  conns[connfd] = nullptr;
  _curr_conns++;
  pthread_mutex_unlock(&_conns_mutex);
}

// 得到当前链接的刻度
void tcp_server::get_conn_num(int *curr_conn) {
  pthread_mutex_lock(&_conns_mutex);
  *curr_conn = _curr_conns;
  pthread_mutex_unlock(&_conns_mutex);
}

// 消息分发路由
msg_router tcp_server::router;

void accept_callback(event_loop *loop, int fd, void *args) {
  tcp_server *server = (tcp_server *)args;
  server->do_accept();
}

tcp_server::tcp_server(event_loop *loop, const char *ip, uint16_t port) {
  bzero(&_connaddr, sizeof(_connaddr));

  // 忽略一些信号 SIGHUP SIGPIPE
  // SIGPIPE:如果客户端关闭，服务端再次write就会产生
  // SIGHUP:如果terminal关闭，会给当前进程发送该信号
  if (signal(SIGHUP, SIG_IGN) == SIG_ERR) {
    fprintf(stderr, "signal ignore SIGHUP\n");
  }

  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    fprintf(stderr, "signal ignore SIGPIPE\n");
  }

  // 创建socket
  _sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
  if (_sockfd == -1) {
    fprintf(stderr, "tcp_server::socket()\n");
    exit(1);
  }

  // 初始化地址
  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  inet_aton(ip, &server_addr.sin_addr);
  server_addr.sin_port = htons(port);

  // 可以多次监听 设置REUSE属性
  int op = 1;
  if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op)) < 0) {
    fprintf(stderr, "setsocketopt SO_REUADDR\n");
  }

  // 绑定端口
  if (bind(_sockfd, (const struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    fprintf(stderr, "bind error\n");
    exit(1);
  }

  // 监听ip端口
  if (listen(_sockfd, 500) == -1) {
    fprintf(stderr, "listen error\n");
    exit(1);
  }

  // 将_socketfd 添加到event_loop中
  _loop = loop;

  // ============== 创建链接管理 ================
  _max_conns = MAX_CONNS;
  // 创建链接信息数组
  conns = new tcp_conn
      *[_max_conns +
        3]; // 3是因为stdin,stdout,stderr
            // 已经被占用，再新开fd一定是从3开始,所以不加3就会栈溢出
  if (conns == nullptr) {
    fprintf(stderr, "new cons[%d] error\n", _max_conns);
    exit(1);
  }

  // 注册_socket读事件 --> accept处理
  _loop->add_io_event(_sockfd, accept_callback, EPOLLIN, this);
}

// 开始提供创建链接服务
void tcp_server::do_accept() {
  int connfd;
  while (true) {
    // accept与客户端创建链接
    printf("begin accept\n");
    connfd = accept(_sockfd, (struct sockaddr *)&_connaddr, &_addrlen);
    if (connfd == -1) {
      if (errno == EINTR) {
        fprintf(stderr, "accept errno=EINTR\n");
        continue;
      } else if (errno == EMFILE) {
        //建立链接过多，资源不够
        fprintf(stderr, "accept errno=EMFILE\n");
      } else if (errno == EAGAIN) {
        fprintf(stderr, "accept errno=EAGAIN\n");
        break;
      } else {
        fprintf(stderr, "accept error");
        exit(1);
      }
    } else {
      // accecpt succ
      int cur_conns = 0;
      get_conn_num(&cur_conns);

      // 判断链接数量
      if (cur_conns >= _max_conns) {
        fprintf(stderr, "so many connections, max = %d\n", _max_conns);
        close(connfd);
      } else {
        tcp_conn *conn = new tcp_conn(connfd, _loop);
        if (conn == nullptr) {
          fprintf(stderr, "new tcp_conn error\n");
          exit(1);
        }
        // 这样，每次accept成功之后，创建一个与当前客户端套接字绑定的tcp_conn对象。
        // 在构造里就完成了基本的对于EPOLLIN事件的监听和回调动作.
        printf("get new connection succ!\n");
      }
      break;
    }
  }
}

// 链接对象释放的析构
tcp_server::~tcp_server() { close(_sockfd); }
