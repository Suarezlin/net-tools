#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include "EventLoop.h"
#include <cstring>

int main() {
  ziniu::EventLoop event_loop;
  int fd = open("/home/linziniu/test", O_RDONLY | O_NONBLOCK);
  if (fd <= 0) {
    std::cerr << "Failed to open /home/linziniu/test: " << std::strerror(errno) << std::endl;
    exit(errno);
  }

  event_loop.EnableRead(fd, [](const ziniu::Event &event) {
    char buf[1024];
    int len = 0;
    do {
      len = read(event.fd, buf, 1024);
      if (len > 0) {
        buf[len] = '\0';
        std::cout << buf << std::endl;
      }
    } while (len != 0);
  });

  event_loop.Start();

  int x = 0;
  std::cin >> x;

  event_loop.Stop();

  close(fd);
  return 0;
}
