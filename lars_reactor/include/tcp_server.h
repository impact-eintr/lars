#pragma once
#include <cstdint>
#include <netinet/in.h>
#include <sys/socket.h>

class tcp_server {
public:
  // server的构造函数
  tcp_server(const char *ip, uint16_t port);

  // 开始提供创建链接服务
  void do_accept();

  // 链接对象的析构
  ~tcp_server();

private:
  int _sockfd;                  // 套接字
  struct sockaddr_in _connaddr; // 客户端链接地址
  socklen_t _addrlen;           // 客户端链接地址长度
};
