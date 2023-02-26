#ifndef NET_TOOLS_INCLUDE_TCPSERVER_H_
#define NET_TOOLS_INCLUDE_TCPSERVER_H_

#include "EventLoop.h"
#include "WorkThread.h"
#include <vector>
#include <memory>
#include <mutex>

namespace ziniu {

const int port = 8080;
const int MAX_WORK_THREAD_NUM = 10;
const int MAX_CLIENT_NUM = 1024;

class TcpServer {
 public:
  TcpServer();
  ~TcpServer();

  void Stop();

 private:
  int listen_fd;
  EventLoop event_loop;
  static std::shared_ptr<WorkThread> work_threads[MAX_WORK_THREAD_NUM];
  static std::vector<int> connections;
  static int curr_thread;
  static std::mutex mutex;

  static void OnConnectionCreateCallback(EventLoop &loop, const Event &event, void *args);

  static void OnAcceptCallback(EventLoop &loop, const Event &event, void *args);

  static void OnConnectionReadCallback(EventLoop &loop, const Event &event, void *args);
};

}

#endif //NET_TOOLS_INCLUDE_TCPSERVER_H_
