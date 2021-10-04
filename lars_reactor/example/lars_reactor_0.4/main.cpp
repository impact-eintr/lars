#include "tcp_server.h"

using namespace std;

int main(int argc, char *argv[]) {
  event_loop loop;
  tcp_server server(&loop, "127.0.0.1", 7777);
  loop.event_process();

  return 0;
}
