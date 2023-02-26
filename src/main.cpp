#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include "TcpServer.h"
#include <cstring>
#include <csignal>

ziniu::TcpServer server{};

void signalHandler(int signum) {
  server.Stop();
  exit(signum);
}

int main() {
  signal(SIGINT, signalHandler);

  int x = 0;
  std::cin >> x;

  return 0;
}