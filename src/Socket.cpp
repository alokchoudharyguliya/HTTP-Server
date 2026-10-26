#include "Socket.h"
#include "Logger.h"
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include<poll.h>
void Socket::create() {
  socketFd_ = socket(AF_INET, SOCK_STREAM, 0);
  // if (socketFd_ == -1)
  //   std::cerr << "Failed to create socket\n";
  if (socketFd_ == -1)
    Logger::error("Failed to create socket: " + std::string(strerror(errno)));
  else
    Logger::debug("Socket created, fd=" + std::to_string(socketFd_));
  // std::cout << "Socket FD = " << socketFd_ << "\n";
}
void Socket::bind(int port) {
  setReuseAddress(true);
  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port);

  // if (::bind(socketFd_, reinterpret_cast<sockaddr *>(&serverAddr),
  //            sizeof(serverAddr)) == -1)
  //   std::cerr << "Failed to bind socket\n";
  if (::bind(socketFd_, reinterpret_cast<sockaddr *>(&serverAddr),
             sizeof(serverAddr)) == -1)
    Logger::error("Failed to bind port " + std::to_string(port) + ": " +
                  strerror(errno));
}

void Socket::listen(int backlog) {
  // if (::listen(socketFd_, backlog) == -1)
  //   std::cerr << "Failed to listen on socket\n";
  // listen():
  if (::listen(socketFd_, backlog) == -1)
    Logger::error("Failed to listen: " + std::string(strerror(errno)));
}
Socket Socket::accept() {
  int clientFd = ::accept(socketFd_, nullptr, nullptr);
  if (clientFd == -1) {
    if (errno != EAGAIN && errno != EWOULDBLOCK)
      Logger::error("Failed to accept client: " + std::string(strerror(errno)));
    return Socket();
  }
  return Socket(clientFd);
}
void Socket::close() {
  if (socketFd_ != -1) {
    ::close(socketFd_);
    socketFd_ = -1;
  }
}
ssize_t Socket::receive(void *buffer, size_t length) {
  return recv(socketFd_, buffer, length, 0);
}
ssize_t Socket::send(const void *buffer, size_t length) {
  const char *data = static_cast<const char *>(buffer);
  size_t sentTotal = 0;
  while (sentTotal < length) {
    ssize_t n = ::send(socketFd_, data + sentTotal, length - sentTotal, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // The socket is non-blocking and the kernel send buffer is full.
        // Wait until it can take more bytes, then continue this write.
        // This waits inside send, so the kqueue loop does not run other clients until it returns.
        pollfd ready{};
        ready.fd = socketFd_;
        ready.events = POLLOUT;
        ::poll(&ready, 1, -1);
        continue;
      }
      return -1;
    }
    if (n == 0)
      return -1;
    sentTotal += static_cast<size_t>(n);
  }
  return static_cast<ssize_t>(sentTotal);
}
Socket::Socket(Socket &&other) noexcept : socketFd_(other.socketFd_) {
  other.socketFd_ = -1;
}

Socket &Socket::operator=(Socket &&other) noexcept {
  if (this != &other) {
    close();
    socketFd_ = other.socketFd_;
    other.socketFd_ = -1;
  }
  return *this;
}

bool Socket::isValid() const { return socketFd_ != -1; }

void Socket::setReuseAddress(bool enable) {
  int opt = enable ? 1 : 0;
  ::setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

int Socket::fd() const { return socketFd_; }

void Socket::setReceiveTimeout(int seconds) {
  timeval tv{};
  tv.tv_sec = seconds;
  ::setsockopt(socketFd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

// Every HTTP Request header ends with \r\n\r\n
// GET / HTTP/1.1\r\n
// Host: localhost\r\n
// User-Agent: Chrome\r\n
// Accept: */*\r\n
// \r\n -> Last line, marks headers finished

// accept and recv return EAGAIN instead of sleeping when the socket has nothing ready.
void Socket::setNonBlocking() {
  int flags = ::fcntl(socketFd_, F_GETFL, 0);
  ::fcntl(socketFd_, F_SETFL, flags | O_NONBLOCK);
}