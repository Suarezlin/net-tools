#include "EventLoop.h"
#include <sys/epoll.h>
#include <iostream>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <sys/eventfd.h>

namespace ziniu {

const int MAX_EVENT_SIZE = 128;

EventLoop::EventLoop() {
  epoll_fd = epoll_create1(0);
#ifndef NDEBUG
  if (epoll_fd < 0) {
    std::cerr << "Failed to create epoll fd: " << std::strerror(errno) << std::endl;
  }
#endif
  signal_fd = eventfd(0, EFD_NONBLOCK);
  if (signal_fd < 0) {
    std::cerr << "Failed to create signal fd: " << std::strerror(errno) << std::endl;
  }
  EnableRead(signal_fd, [](EventLoop &loop, const Event &event, void *args) {
    int fd = event.fd;
    unsigned long long res = 0;
    int ret = read(fd, &res, sizeof(unsigned long long));
    if (ret != sizeof(unsigned long long)) {
      std::cerr << "Failed to read signal fd: " << std::strerror(errno) << std::endl;
    }
  });
}

EventLoop::~EventLoop() {
  if (is_start.load() && !is_exit.load()) {
    Stop();
    listen_events.clear();
  }
}

// TODO: thread creation error handle
void EventLoop::Start() {
  std::lock_guard<std::recursive_mutex> lock(mutex);
  if (!is_start.load()) {
    is_start.store(true);
    working_thread = std::thread(&EventLoop::ProcessEvent, this);
  } else {
#ifndef NDEBUG
    std::cerr << "EventLoop has already started" << std::endl;
#endif
  }
}

void EventLoop::Stop() {
  std::lock_guard<std::recursive_mutex> lock(mutex);
  if (is_exit.load()) {
#ifndef NDEBUG
    std::cerr << "EventLoop has already exited" << std::endl;
#endif
    return;
  } else {
    if (is_start.load()) {
      is_exit.store(true);
      WakeUp();
      working_thread.join();
    } else {
#ifndef NDEBUG
      std::cerr << "EventLoop does not start" << std::endl;
#endif
    }
  }
}

Result EventLoop::EnableRead(int fd, ziniu::Event::Callback callback, void *args) {
  std::lock_guard<std::recursive_mutex> lock(mutex);
  return Enable(fd, EPOLLIN, callback, args);
}

Result EventLoop::EnableWrite(int fd, ziniu::Event::Callback callback, void *args) {
  std::lock_guard<std::recursive_mutex> lock(mutex);
  return Enable(fd, EPOLLOUT, callback, args);
}

Result EventLoop::DisableRead(int fd) {
  std::lock_guard<std::recursive_mutex> lock(mutex);
  return Disable(fd, EPOLLIN);
}

Result EventLoop::DisableWrite(int fd) {
  std::lock_guard<std::recursive_mutex> lock(mutex);
  return Disable(fd, EPOLLOUT);
}

Result EventLoop::RemoveEvent(int fd) {
  std::lock_guard<std::recursive_mutex> lock(mutex);
  if (listen_events.find(fd) != listen_events.end()) {
    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    if (ret != 0) {
#ifndef NDEBUG
      std::cerr << "Failed to remove event for fd " << fd << std::strerror(errno) << std::endl;
#endif
      return Result::FAIL;
    }
    listen_events.erase(fd);
    return Result::SUCCESS;
  } else {
#ifndef NDEBUG
    std::cerr << "Failed to remove event for fd " << fd << ": fd not exists" << std::endl;
#endif
    return Result::FAIL;
  }
}

bool EventLoop::Ready() const {
  return epoll_fd <= 0 || (is_start.load() && !is_exit.load());
}

void EventLoop::ProcessEvent() {
  assert(epoll_fd > 0);

  // TODO: Handle max event size
  epoll_event ready_events[MAX_EVENT_SIZE];
  while (!is_exit.load()) {
    int nfds = epoll_wait(epoll_fd, ready_events, MAX_EVENT_SIZE, 100);
    if (is_exit.load()) {
      break;
    }
    if (nfds == -1) {
#ifndef NDEBUG
      std::cerr << "Failed to wait epoll: " << std::strerror(errno) << std::endl;
#endif
      break;
    }
    decltype(listen_events.begin()) it;
    for (int i = 0; i < nfds; i++) {
      it = listen_events.find(ready_events[i].data.fd);
      assert(it != listen_events.end());
      uint32_t ready_event = ready_events[i].events;
//      std::cout << ready_event << std::endl;
      if (ready_event & EPOLLIN) {
        listen_events[ready_events[i].data.fd].read_callback(*this,
                                                             listen_events[ready_events[i].data.fd],
                                                             it->second.args);
      } else if (ready_event & EPOLLOUT) {
        listen_events[ready_events[i].data.fd].write_callback(*this,
                                                              listen_events[ready_events[i].data.fd],
                                                              it->second.args);
      } else if (ready_event & (EPOLLERR | EPOLLHUP)) {
#ifndef NDEBUG
        std::cout << "Handle " << ready_events[i].data.fd << " EPOLLERR/EPOLLHUP" << std::endl;
#endif
        mutex.lock();
        close(ready_events[i].data.fd);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ready_events[i].data.fd, nullptr);
        listen_events.erase(ready_events[i].data.fd);
        mutex.unlock();
      } else {
        std::cout << "Unknown event: " << ready_event << std::endl;
      }
    }
    if (is_exit.load()) {
      break;
    }
  }

  exited.store(true);
  close(epoll_fd);
  close(signal_fd);
  for (auto &it : listen_events) {
    close(it.first);
  }
  std::cout << "thread exit" << std::endl;
}

Result EventLoop::Enable(int fd, uint32_t event_type, Event::Callback callback, void *args) {
  if (event_type != EPOLLIN && event_type != EPOLLOUT) {
    return Result::FAIL;
  }
  std::string op = event_type == EPOLLIN ? "read" : "write";
  if (callback == nullptr) {
#ifndef NDEBUG
    std::cerr << "Callback cannot be nullptr" << std::endl;
#endif
    return Result::FAIL;
  }
  Event event;
  auto it = listen_events.find(fd);
  int epoll_operation{0};
  if (it != listen_events.end()) {
    event = it->second;
    event.mask |= event_type;
    if (event_type == EPOLLIN) {
      event.read_callback = callback;
    } else if (event_type == EPOLLOUT) {
      event.write_callback = callback;
    }
    event.args = args;
    epoll_operation = EPOLL_CTL_MOD;
  } else {
    if (event_type == EPOLLIN) {
      event = Event{fd, event_type, callback, nullptr, args};
    } else if (event_type == EPOLLOUT) {
      event = Event{fd, event_type, nullptr, callback, args};
    }
    epoll_operation = EPOLL_CTL_ADD;
  }

  epoll_event epoll_event_{};
  epoll_event_.events = event.mask;
  epoll_event_.data.fd = fd;
  int ret = epoll_ctl(epoll_fd, epoll_operation, fd, &epoll_event_);
  if (ret != 0) {
#ifndef NDEBUG
    std::cerr << "Failed to enable " << op << " " << fd << ": " << std::strerror(errno) << std::endl;
#endif
    return Result::FAIL;
  }
  listen_events[fd] = event;
  return Result::SUCCESS;
}

Result EventLoop::Disable(int fd, uint32_t event_type) {
  std::string op = event_type == EPOLLIN ? "read" : "write";
  auto it = listen_events.find(fd);
  if (it != listen_events.end()) {
    Event listen_event = it->second;
    int epoll_operation{};
    listen_event.mask &= (~event_type);
    if (event_type == EPOLLIN) {
      listen_event.read_callback = nullptr;
    } else {
      listen_event.write_callback = nullptr;
    }
    if (listen_event.mask == 0) {
      epoll_operation = EPOLL_CTL_DEL;
    } else {
      epoll_operation = EPOLL_CTL_MOD;
    }
    epoll_event event{};
    event.events = listen_event.mask;
    int ret = epoll_ctl(epoll_fd, epoll_operation, fd, &event);
    if (ret != 0) {
#ifndef NDEBUG
      std::cerr << "Failed to disable " << event_type << " for fd " << fd << std::strerror(errno) << std::endl;
#endif
      return Result::FAIL;
    }
    if (epoll_operation == EPOLL_CTL_DEL) {
      listen_events.erase(fd);
    } else {
      listen_events[fd] = listen_event;
    }
    return Result::SUCCESS;
  } else {
#ifndef NDEBUG
    std::cerr << "Failed to disable " << event_type << " for fd " << fd << ": fd not exists" << std::endl;
#endif
    return Result::FAIL;
  }
}

void EventLoop::WakeUp() {
  std::lock_guard<std::recursive_mutex> lock{mutex};
  unsigned long long val = 1;
  if (!exited.load()) {
    int ret = write(signal_fd, &val, sizeof(unsigned long long));
    if (ret != sizeof(unsigned long long)) {
      std::cerr << "Failed to write signal fd: " << std::strerror(errno) << std::endl;
    }
  }
}

}