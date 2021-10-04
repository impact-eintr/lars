#ifndef NET_CONNECTION_H_
#define NET_CONNECTION_H_

// 网络通信的抽象类 任何需要进行收发消息的模块 都可以实现该类

class net_connection {
public:
  net_connection() : param(nullptr) {}
  // 发送消息的接口
  virtual int send_message(const char *data, int datalen, int msgid) = 0;
  virtual int get_fd() = 0;

  void *param; // TCP client 可以通过该参数传递一些自定义的参数
};

#endif // NET_CONNECTION_H_
