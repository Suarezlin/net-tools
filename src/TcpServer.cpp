#include "TcpServer.h"
#include "WorkThread.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <algorithm>
#include <unistd.h>

namespace ziniu {

const int BUFFER_SIZE = 1024;

std::shared_ptr<WorkThread> TcpServer::work_threads[MAX_WORK_THREAD_NUM]{};
std::vector<int> TcpServer::connections{};
int TcpServer::curr_thread = 0;
std::mutex TcpServer::mutex{};

void TcpServer::OnConnectionCreateCallback(EventLoop &loop, const Event &event, void *args) {
  auto *thread = reinterpret_cast<WorkThread *>(args);
  thread->Lock();
  std::queue<int> &queue = thread->getQueue();
  unsigned long long res;
  int ret = read(thread->getEventFd(), &res, sizeof(unsigned long long));
  if (ret == -1) {
    std::cerr << "Failed to read event fd: " << std::strerror(errno) << std::endl;
    return;
  }
  while (!queue.empty()) {
    int fd = queue.front();
    queue.pop();
    TcpServer::connections.push_back(fd);
    thread->GetEventLoop()->EnableRead(fd, OnConnectionReadCallback, thread);
  }
  thread->UnLock();
}

void TcpServer::OnConnectionReadCallback(EventLoop &loop, const Event &event, void *args) {
  auto *thread = static_cast<WorkThread *>(args);
  int connection_fd = event.fd;
  ssize_t ret = 0;
  char in_buf[BUFFER_SIZE];
  ret = read(connection_fd, in_buf, BUFFER_SIZE);
  if (ret > 0) {
    std::string out(in_buf, ret - 1);
    write(connection_fd, in_buf, ret);
    std::cout << "Connection " << connection_fd << " recv " << ret << " bytes data: " << out << std::endl;
  } else {
    std::cout << "Connection " << connection_fd << " close" << std::endl;
    thread->GetEventLoop()->RemoveEvent(connection_fd);
    close(connection_fd);
    std::remove(TcpServer::connections.begin(), TcpServer::connections.end(), connection_fd);
  }
}

void TcpServer::OnAcceptCallback(EventLoop &loop, const Event &event, void *args) {
  int fd = event.fd;
  sockaddr addr{};
  socklen_t len = sizeof(addr);
  int connection_fd = accept(fd, &addr, &len);
  if (connection_fd == -1) {
    std::cerr << "Failed to accept: " << std::strerror(errno) << std::endl;
    exit(-1);
  }
  std::cout << "New connection " << connection_fd << std::endl;
  TcpServer::mutex.lock();
  int work_thread = (TcpServer::curr_thread + 1) % MAX_WORK_THREAD_NUM;
  TcpServer::curr_thread = work_thread;
  TcpServer::mutex.unlock();
  TcpServer::work_threads[work_thread]->SendEvent(connection_fd);
}

TcpServer::TcpServer() : event_loop() {
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd == -1) {
    std::cerr << "Failed create socket: " << std::strerror(errno) << std::endl;
    exit(-1);
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  int flag = 1;

  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  if (bind(listen_fd, (struct sockaddr *) &address, sizeof(address)) == -1) {
    std::cerr << "Failed bind socket: " << std::strerror(errno) << std::endl;
    exit(-1);
  }

  if (listen(listen_fd, MAX_CLIENT_NUM) == -1) {
    std::cerr << "Failed listen socket: " << std::strerror(errno) << std::endl;
    exit(-1);
  }

  for (int i = 0; i < MAX_WORK_THREAD_NUM; i++) {
    work_threads[i] = std::make_shared<WorkThread>();
    work_threads[i]->Start(OnConnectionCreateCallback);
  }

  Result ret = event_loop.EnableRead(listen_fd, OnAcceptCallback);

  if (ret == Result::FAIL) {
    std::cerr << "Failed to listen socket accept in epoll" << std::endl;
    exit(-1);
  }

  event_loop.Start();

  std::cout << "Init tcp server successfully" << std::endl;
}

void TcpServer::Stop() {
  std::cout << "TcpServer exiting..." << std::endl;
  for (auto &it : connections) {
    close(it);
  }
  for (auto &it : work_threads) {
    it->Stop();
  }
  event_loop.Stop();
  connections.clear();
}

TcpServer::~TcpServer() {
  if (event_loop.Ready()) {
    Stop();
  }
}

}
