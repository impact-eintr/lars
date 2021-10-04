#include "tcp_client.h"
#include <arpa/inet.h>
#include <asm-generic/errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include "unistd.h"

static void connection_delay(event_loop *loop, int fd, void *args);

static void write_callback(event_loop *loop, int fd, void *args);

static void read_callback(event_loop *loop, int fd, void *args);

//初始化客户端套接字
tcp_client::tcp_client(event_loop *loop, const char *ip, unsigned short port, const char *name):
  _ibuf(m4M),
  _obuf(m4M){
  _sockfd = -1;
  _msg_callback = nullptr;
  _name = name;
  _loop = loop;

  bzero(&_server_addr, sizeof(_server_addr));

  _server_addr.sin_family = AF_INET;
  inet_aton(ip, &_server_addr.sin_addr);
  _server_addr.sin_port = htons(port);

  _addrlen = sizeof(_server_addr);

  this->do_connect();
} // ​ 这里初始化tcp_client链接信息，然后调用do_connect()创建链接.

// 创建链接
void tcp_client::do_connect() {
  if (_sockfd != -1) {
    close(_sockfd);
  }

  // 创建套接字
  _sockfd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, IPPROTO_TCP);
  if (_sockfd == -1) {
    fprintf(stderr, "create tcp client docket error\n");
    exit(1);
  }

  int ret = connect(_sockfd, (const struct sockaddr*)&_server_addr, _addrlen);
  if (ret == 0) {
    // 创建链接成功
    connected = true;

    // 注册读回调
    _loop->add_io_event(_sockfd, read_callback, EPOLLIN, this);
    // 如果写缓冲区有数据，那么也需要触发写回调
    if (this->_obuf.length != 0) {
      _loop->add_io_event(_sockfd, write_callback, EPOLLIN, this);
    }
    printf("connect %s:%d succ!\n", inet_ntoa(_server_addr.sin_addr), ntohs(_server_addr.sin_port));
  } else {
    if (errno == EINPROGRESS) {
      // fd是非阻塞的 可能会出现这个错误 但并不代表链接创建失败
      // 如果fd是可写状态，则认为链接是创建成功的
      fprintf(stderr, "do_connect EINPROCESS\n");
      // 让event_loop去触发一个创建判断链接业务 有EPOLLOUT事件立刻触发
      // 这里是又触发一个写事件，直接让程序流程跳转到connection_delay()方法.
      // 那么我们需要在里面判断链接是否已经判断成功，并且做出一定的创建成功之后的业务动作。
      _loop->add_io_event(_sockfd, connection_delay, EPOLLOUT, this);
    } else {
      fprintf(stderr, "connection error\n");
      exit(1);
    }
  }
}

// 判断链接是否是创建链接，主要是针对非阻塞socket 返回EINPROGRESS错误
static void connection_delay(event_loop *loop, int fd, void *args) {
  tcp_client *cli = (tcp_client*)args;
  loop->del_io_event(fd);

  int result = 0;
  socklen_t result_len = sizeof(result);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len);
  if (result == 0) {
    // 链接是建立成功的
    cli->connected = true;

    printf("connect %s:%d succ!\n", inet_ntoa(cli->_server_addr.sin_addr),
           ntohs(cli->_server_addr.sin_port));

    // 建立连接成功之后，主动发送send_message
    const char *msg = "hello lars!";
    int msgid = 1;
    cli->send_message(msg, strlen(msg), msgid);

    loop->add_io_event(fd, read_callback, EPOLLIN, cli);

    if (cli->_obuf.length != 0) {
      // 输出缓冲有数据可写
      loop->add_io_event(fd, write_callback, EPOLLOUT, cli);
    }
  } else {
    // 链接创建失败
    fprintf(stderr, "connection %s:%d error\n",
            inet_ntoa(cli->_server_addr.sin_addr),
            ntohs(cli->_server_addr.sin_port));
  }
}

static void write_callback(event_loop *loop, int fd, void *args) {

}

static void read_callback(event_loop *loop, int fd, void *args) {

}


// 发送amessage方法
int tcp_client::send_message(const char *data, int msglen, int msgid) {

}

// 处理读业务
int tcp_client::do_read() {

}

// 处理写业务
int tcp_client::do_write() {

}

// 释放链接资源
void tcp_client::clean_conn() {

}

tcp_client::~tcp_client() {

}
