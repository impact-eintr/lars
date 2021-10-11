#include "tcp_client.h"
#include <cstdint>

void echo(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data) {
  printf("得到服务端回执的数据\n");
  printf("recv server: [%s]\n", data);
  printf("msgid: [%d]\n", msgid);
  printf("len: [%d]\n", len);
}

int main() {
  event_loop loop;

  // 创建tcp客户端
  tcp_client client(&loop, "127.0.0.1", 7777, "clientv0.0.6");

  client.add_msg_router(1, echo);
  client.add_msg_router(2, echo);

  loop.event_process();

  return 0;
}
