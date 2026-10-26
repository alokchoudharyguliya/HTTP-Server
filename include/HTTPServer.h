#pragma once
#include "FileServer.h"
#include "HTTPParser.h"
#include "Socket.h"
#include "ThreadPool.h"
#include <unordered_map>

struct Connection {
  Socket socket;
  std::string buffer;
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
  bool handleOneRequest(Socket &client, std::string &requestBuffer);
  void readReady(int fd);
  void acceptReady();
  // void handleClient(Socket client);
  void start();
};