#include <string.h>
#include <string>

#include "config_file.h"
#include "echoMessage.pb.h"
#include "net_connection.h"
#include "tcp_server.h"

// 回显业务的回调函数
void callback_echo(const char *data, uint32_t len, int msgid,
                   net_connection *conn, void *user_data) {
  qps_test::EchoMessage request, response;

  //解包，确保data[0-len]是一个完整包
  request.ParseFromArray(data, len);
  std::cout << request.id() << " 获取数据为: " << request.content()
            << std::endl;

  //设置新pb包
  response.set_id(request.id());
  response.set_content(request.content());

  //序列化
  std::string responseString;
  response.SerializeToString(&responseString);

  conn->send_message(responseString.c_str(), responseString.size(), msgid);
}

int main() {
  event_loop loop;

  //加载配置文件
  config_file::setPath("./serv.conf");
  std::string ip =
      config_file::instance().GetString("reactor", "ip", "0.0.0.0");
  short port = config_file::instance().GetNumber("reactor", "port", 7777);

  printf("ip = %s, port = %d\n", ip.c_str(), port);

  tcp_server server(&loop, ip.c_str(), port);

  //注册消息业务路由
  server.add_msg_router(1, callback_echo);

  loop.event_process();

  return 0;
}
