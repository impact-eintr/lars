#include "tcp_server.h"

using namespace std;

int main(int argc, char *argv[]) {
  tcp_server server("127.0.0.1", 7777);
  server.do_accept();

  return 0;
}
