#include "WorkThread.h"
#include <sys/eventfd.h>
#include <cstring>
#include <utility>
#include <unistd.h>

namespace ziniu {

WorkThread::WorkThread() {
  event_fd = eventfd(0, EFD_NONBLOCK);
  if (event_fd < 0) {
    std::cerr << "Failed to create event fd: " << std::strerror(errno) << std::endl;
    exit(-1);
  }
}

void WorkThread::Start(Event::Callback event_fd_callback) {
  event_loop.EnableRead(event_fd, std::move(event_fd_callback), this);
  event_loop.Start();
}

void WorkThread::Stop() {
  event_loop.Stop();
  close(event_fd);
}

void WorkThread::SendEvent(const int &event) {
  std::lock_guard<std::mutex> lock_guard{mutex};
  unsigned long long msg = 1;
  queue.push(event);
  int ret = write(event_fd, &msg, sizeof(msg));
  if (ret < 0) {
    std::cerr << "Failed to write event fd" << std::endl;
  }
}

void WorkThread::Lock() {
  mutex.lock();
}
void WorkThread::UnLock() {
  mutex.unlock();
}

}
