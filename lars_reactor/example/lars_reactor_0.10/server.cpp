#include "udp_server.h"
#include <string.h>
#include <string>
#include "config_file.h"

using namespace std;

// 回显业务的回调函数
void callback_echo(const char *data, uint32_t len, int msgid,
                   net_connection *conn, void *user_data) {
  printf("callback_echo %s...\n", data);
  // 直接回显
  conn->send_message(data, len, msgid);
}

int main(int argc, char *argv[]) {
  event_loop loop;

  // 加载配置文件
  config_file::setPath("./serv.conf");
  std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
  short port = config_file::instance()->GetNumber("reactor", "port", 7777);

  udp_server server(&loop, ip.c_str(), port);

  // 注册消息业务路由
  server.add_msg_router(1, callback_echo);

  loop.event_process();

  return 0;
}
