#ifndef NET_TOOLS_INCLUDE_WORKTHREAD_H_
#define NET_TOOLS_INCLUDE_WORKTHREAD_H_

#include "EventLoop.h"
#include <thread>
#include <mutex>
#include <queue>

namespace ziniu {

class WorkThread {
 public:
  WorkThread();
  WorkThread(const WorkThread &) = delete;
  WorkThread &operator=(const WorkThread &) = delete;

  EventLoop *GetEventLoop() { return &event_loop; }

  std::queue<int> &getQueue() { return queue; }

  int getEventFd() const { return event_fd; }

  void SendEvent(const int &event);

  void Start(Event::Callback event_fd_callback);
  void Stop();

  void Lock();
  void UnLock();

 private:
  int event_fd;
  std::mutex mutex{};
  std::queue<int> queue{};
  EventLoop event_loop{};
};
}

#endif //NET_TOOLS_INCLUDE_WORKTHREAD_H_
