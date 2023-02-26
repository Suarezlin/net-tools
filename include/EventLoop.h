#ifndef NET_TOOLS_INCLUDE_EVENTLOOP_H_
#define NET_TOOLS_INCLUDE_EVENTLOOP_H_

#include <functional>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include <iostream>

namespace ziniu {

class EventLoop;

enum class Result {
  SUCCESS,
  FAIL
};

class Event {
 public:

  using Callback = std::function<void(EventLoop &, const Event &, void *)>;

  Event() = default;

  Event(int fd,
        uint32_t mask,
        Callback read_callback = nullptr,
        Callback write_callback = nullptr,
        void *args = nullptr)
      : fd(fd),
        mask(mask),
        read_callback(std::move(read_callback)),
        write_callback(std::move(write_callback)),
        args(args) {}

  int fd{};
  uint32_t mask{};
  Callback read_callback;
  Callback write_callback;
  void *args;
};

class EventLoop {
 public:
  EventLoop();
  ~EventLoop();

  EventLoop(const EventLoop &) = delete;
  EventLoop &operator=(const EventLoop &) = delete;

  void Start();

  void Stop();

  Result EnableRead(int fd, Event::Callback callback, void *args = nullptr);

  Result EnableWrite(int fd, Event::Callback callback, void *args = nullptr);

  Result DisableRead(int fd);

  Result DisableWrite(int fd);

  Result RemoveEvent(int fd);

  bool Ready() const;

 private:
  int epoll_fd{};
  int signal_fd{};
  std::unordered_map<int, Event> listen_events{};
  std::atomic_bool is_start{false};
  std::atomic_bool is_exit{false};
  std::atomic_bool exited{false};
  std::thread working_thread;
  std::recursive_mutex mutex;

  void ProcessEvent();

  Result Enable(int fd, uint32_t event_type, Event::Callback callback, void *args = nullptr);

  Result Disable(int fd, uint32_t event_type);

  void WakeUp();
};

}

#endif //NET_TOOLS_INCLUDE_EVENTLOOP_H_
