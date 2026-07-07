#include "HTTPServer.h"
#include "Logger.h"
#include <iostream>
#include <stdlib.h>
#include <string>
int main(int argc, char *argv[]) {
  int port = 8001;
  std::string root = "public";
  // if(argc<3){
  // Logger::warning("Usage: ./http_server <port> <documentRoot>");
  // std::cout<<"Helper Message: use ./http_server <port> <documentRoot>\n";
  // return 1;
  // }
  if (argc >= 2) {
    try {
      port = std::stoi(argv[1]);
    } catch (...) {
      Logger::error("Invalid port: " + std::string(argv[1]));
    }
  }
  if (argc >= 3)
    root = argv[2];
  HTTPServer server(port, root);
  server.start();
}
