#pragma once
#include "FileServer.h"
#include "HTTPParser.h"
#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "Socket.h"
#include "ThreadPool.h"
#include <chrono>
#include <unordered_map>

struct Connection {
  Socket socket;
  std::string buffer;
  std::string outgoing;
  std::ifstream file;
  bool responding = false;
  bool shouldClose = false;
  bool writeWatched = false;
  bool streaming = false;
  size_t bytesSent = 0;
  HTTPRequest request;
  HTTPResponse response;
  std::chrono::steady_clock::time_point started{};
};

class HTTPServer {

private:
  std::unordered_map<int, Connection> connections_;
  int kq_ = -1;
  FileServer fs_;
  HTTPParser parser_;
  int port_;
  Socket listener_;
  std::string documentRoot_;
  ThreadPool threadPool_;

public:
  HTTPServer(int port, const std::string &root);
  bool handleOneRequest(Connection &conn);
  void readReady(int fd);
  void writeReady(int fd);
  void progress(int fd);
  bool flush(Connection &conn);
  void acceptReady();
  // void handleClient(Socket client);
  void start();
};