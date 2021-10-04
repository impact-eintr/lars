#include "tcp_server.h"
#include "buf_pool.h"
#include "io_buf.h"
#include "reactor_buf.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include "tcp_conn.h"

// 临时的收发消息
struct message {
  char data[m4K];
  char len;
};

// 全局消息
struct message msg;

void server_rd_callback(event_loop *loop, int fd, void *args);
void server_wt_callback(event_loop *loop, int fd, void *args);

void server_rd_callback(event_loop *loop, int fd, void *args) {
#ifdef debug
  printf("调用读回调函数\n");
#endif
  int ret = 0;

  struct message *msg = (struct message*)args;
  input_buf ibuf;

  ret = ibuf.read_data(fd);
  if (ret == -1) {
    fprintf(stderr, "ibuf read_data error\n");
    //删除事件
    loop->del_io_event(fd);
    //对端关闭
    close(fd);
    return;
  }
  if (ret == 0) {
    //删除事件
    loop->del_io_event(fd);
    //对端关闭
    close(fd);
    return;
  }

  printf("ibuf.length() = %d\n", ibuf.length());

  // 将读到的数据放到msg中
  msg->len = ibuf.length();
  bzero(msg->data, m4K);
  // TODO
  //bzero(msg->data, msg->len); 原代码
  memcpy(msg->data, ibuf.data(), msg->len);

#ifdef debug
  printf("[R]msg->len %d msg->data %s\n", msg->len, msg->data);
#endif

  ibuf.pop(msg->len);
  ibuf.adjust();

  printf("recv data = %s\n", msg->data);

  // 删除读事件 添加写事件
  loop->del_io_event(fd, EPOLLIN);
  loop->add_io_event(fd, server_wt_callback, EPOLLOUT, msg);

}

// server write_callback
void server_wt_callback(event_loop *loop, int fd, void *args) {
#ifdef debug
  printf("调用写回调函数\n");
#endif
  struct message *msg = (struct message*)args;
  output_buf obuf;

  // 回显数据
  obuf.send_data(msg->data, msg->len);
#ifdef debug
  printf("[W]msg->len %d msg->data %s\n", msg->len, msg->data);
#endif
  while(obuf.length()) {
    int write_ret =obuf.write2fd(fd);
    if (write_ret == -1) {
      fprintf(stderr, "write connfd error\n");
      return;
    } else if (write_ret == 0) {
      // 不是错误 表示此时不可写(发生了EAGAIN)
      break;
    }
  }
  //删除写事件，添加读事件
  loop->del_io_event(fd, EPOLLOUT);
  loop->add_io_event(fd, server_rd_callback, EPOLLIN, msg);
}

void accept_callback(event_loop *loop, int fd, void *args) {
  tcp_server *server = (tcp_server*)args;
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
  // 注册_docket读事件 --> accept处理
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
      tcp_conn *conn = new tcp_conn(connfd, _loop);
      if (conn == nullptr) {
        fprintf(stderr, "new tcp_conn error\n");
        exit(1);
      }
      // 这样，每次accept成功之后，创建一个与当前客户端套接字绑定的tcp_conn对象。
      // 在构造里就完成了基本的对于EPOLLIN事件的监听和回调动作.
      printf("get new connection succ!\n");
      break;
    }
  }
}

// 链接对象释放的析构
tcp_server::~tcp_server() {
  close(_sockfd);
}
