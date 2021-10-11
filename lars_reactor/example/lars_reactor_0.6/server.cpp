#include "tcp_server.h"
#include <string.h>

using namespace std;

// 回显业务的回调函数
void callback_echo(const char *data, uint32_t len, int msgid,
                   net_connection *conn, void *user_data) {
  printf("callback_echo ...\n");
  // 直接回显
  conn->send_message(data, len, msgid);
}

// 打印信息回调函数
void print_echo(const char *data, uint32_t len, int msgid, net_connection *conn,
                void *user_data) {
  printf("print_echo ...\n");
  printf("recv client: [%s]\n", data);
  printf("msgid: [%d]\n", msgid);
  printf("len: [%d]\n", len);
  const char *resp = "hello this is lar-server!";
  conn->send_message(resp, strlen(resp), msgid);
}

int main(int argc, char *argv[]) {
  event_loop loop;
  tcp_server server(&loop, "127.0.0.1", 7777);

  // 注册消息业务路由
  server.add_msg_router(1, callback_echo);
  server.add_msg_router(2, print_echo);

  loop.event_process();

  return 0;
}
