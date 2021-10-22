#include "udp_client.h"
#include <cstdint>
#include <string.h>
#include <stdio.h>


void echo(const char *data, uint32_t len, int msgid, net_connection *conn,
          void *user_data) {
  printf("得到服务端回执的数据\n");
  printf("recv server: [%s]\n", data);
  printf("msgid: [%d]\n", msgid);
  printf("len: [%d]\n", len);
}

int main() {
  event_loop loop;

  // 创建tcp客户端
  udp_client client(&loop, "127.0.0.1", 7777);

  // 注册消息路由业务
  client.add_msg_router(1, echo);

  // 发消息
  int msgid = 1;
  const char *msg = "Hello Lars!";

  client.send_message(msg, strlen(msg), msgid);

  // 开启事件监听
  loop.event_process();

  return 0;
}
