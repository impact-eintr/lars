#ifndef TCP_CONN_H_
#define TCP_CONN_H_
#include "event_loop.h"
#include "reactor_buf.h"
#include "message.h"

// 一个tcp的链接信息
class tcp_conn {
public:
  // 初始化tcp_conn
  tcp_conn(int connfd, event_loop *loop);

  // 处理读业务
  void do_read();

  // 处理写业务
  void do_write();

  // 销毁tcp_conn
  void clean_conn();

  // 发送消息的方法
  int send_message(const char *data, int msglen, int msgid);

private:
  // 当前链接的fd
  int _connfd;
  // 该连接归属的event_poll
  event_loop *_loop;
  // 输出buf
  output_buf obuf;
  // 输入buf
  input_buf ibuf;
};

#endif // TCP_CONN_H_
