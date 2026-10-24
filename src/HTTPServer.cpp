#include "HTTPServer.h"
#include "HTTPResponse.h"
#include "Logger.h"
#include "MimeResolver.h"
#include <chrono>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <utility>
// class HTTPServer{
// 	private:
// 		FileServer fs_;
// 		HTTPParser parser_;
// 		int port_;
// 		Socket listener_;
// 	public:
// 	HTTPServer(int port):port_(port){}
// 	void handleClient(Socket client);
// 	void start();
// };

HTTPServer::HTTPServer(int port, const std::string &root)
    : port_(port), threadPool_(8), documentRoot_(root) {}

void HTTPServer::start() {
  // std::cout << "Starting HTTP Server on port " << port_ << "\n";
  Logger::info("STARTING HTTP Server on Port " + std::to_string(port_) + "\n");
  listener_.create();
  // --------setsockopt()---------
  //   GETSOCKOPT(2)                              System Calls Manual
  //   GETSOCKOPT(2)

  // NAME
  //      getsockopt, setsockopt – get and set options on sockets

  // SYNOPSIS
  //      #include <sys/socket.h>

  //      int
  //      getsockopt(int socket, int level, int option_name, void *restrict
  //      option_value,
  //          socklen_t *restrict option_len);

  //      int
  //      setsockopt(int socket, int level, int option_name, const void
  //      *option_value, socklen_t option_len);

  // DESCRIPTION
  //      getsockopt() and setsockopt() manipulate the options associated with a
  //      socket.  Options may exist at multiple protocol levels; they are
  //      always present at the uppermost “socket” level.

  listener_.bind(port_);
  // std::cout << "Port          : " << port_ << '\n';
  // std::cout << "Document Root : " << documentRoot_ << '\n';
  // std::cout << "Workers       : 8\n";

  Logger::info("Configuration:");
  Logger::info("  Port          : " + std::to_string(port_));
  Logger::info("  Document root : " + documentRoot_);
  Logger::info("  Workers       : 8");
  Logger::info("Listening on port " + std::to_string(port_));
  listener_.listen();
  std::cout << "Server is listening on port " << port_ << "\n";
  while (true) {
    // std::cout << "Waiting for a client\n";
    // Logger::debug("Waiting for a client");
    Socket client = listener_.accept();
    // std::cout << "Client Connected\n";
    Logger::debug("Client Connected");
    // std::thread(&HTTPServer::handleClient,this,std::move(client)).detach();
    Task task([this, client = std::move(client)]() mutable {
      handleClient(std::move(client));
    });
    threadPool_.submit(std::move(task));
  }
}
// void HTTPServer::handleClient(Socket client) {
//   char buffer[4097];
//   while (true) {
//     auto start = std::chrono::high_resolution_clock::now();
//     ssize_t bytesReceived = client.receive(buffer, sizeof(buffer) - 1);
//     if (bytesReceived <= 0) {
//       break;
//     }
//     if (bytesReceived > 0) {
//       buffer[bytesReceived] = '\0';
//       HTTPRequest request = parser_.parse(buffer);
//       std::cout << ">>Method:" << request.method << "\n";
//       std::cout << ">>Path:" << request.path << "\n";
//       std::cout << ">>Version:" << request.version << "\n";

//       std::cout << "Received:\n";
//       std::string filePath;
//       if (request.path == "/")
//         filePath = documentRoot_ + "/index.html";
//       else
//         filePath = documentRoot_ + request.path;
//       HTTPResponse response;
//       if (fs_.exists(filePath))
//         response.body = fs_.readFile(filePath);
//       else {
//         response.statusCode = 404;
//         response.reasonPhrase = "Not Found";
//         response.body = "<h1>404 Not Found</h1>";
//       }

//       // response.headers["Content-Type"]="text/html";
//       response.headers["Content-Type"] = MimeResolver::getMime(filePath);
//       response.headers["Content-Length"] =
//       std::to_string(response.body.size());
//       //   response.headers["Connection"] = "close";
//       std::string rawResponse = response.serialize();
//       client.send(rawResponse.c_str(), rawResponse.size());
//       //   if(request.headers["Connection"] == "close")
//       //   response.headers["Connection"]="close";
//       // else
//       // response.headers["Connection"]="keep-alive";
//       auto end = std::chrono::high_resolution_clock::now();
//       auto duration =
//           std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

//       Logger::log(request, response, duration.count());
//       std::cout << buffer << "\n";
//       for (const auto &header : request.headers)
//         std::cout << header.first << "<" << header.second << "\n";
//     }
//   }
// }

void HTTPServer::handleClient(Socket client) {
  char buffer[4097];
  std::string requestBuffer;
  while (true) {
    auto start = std::chrono::high_resolution_clock::now();
    ssize_t headerEnd = requestBuffer.find("\r\n\r\n");
    while (headerEnd == std::string::npos) // while (request not complete) {
    {
      // ssize_t bytesReceived = client.receive(buffer, sizeof(buffer) - 1);
      // if (!contains "\r\n\r\n") {
      // recv() append into requestBuffer;
      ssize_t bytesReceived = client.receive(buffer, sizeof(buffer) - 1);
      // } else {
      if (bytesReceived <= 0) {
        Logger::debug("Client Disconnected");
        return;
      }
      buffer[bytesReceived] = '\0';
      requestBuffer.append(buffer, bytesReceived);
      // parse();
      // HTTPRequest request = parser_.parse(buffer);
      headerEnd=requestBuffer.find("\r\n\r\n");
    }
    HTTPRequest request = parser_.parse(requestBuffer);
    Logger::debug("Request: " + request.method + " " + request.path + " " +
                  request.version);

    // std::cout << ">>Method:" << request.method << "\n";
    // std::cout << ">>Path:" << request.path << "\n";
    // std::cout << ">>Version:" << request.version << "\n";

    std::cout << "Received:\n";
    std::string filePath;
    if (request.path == "/")
      filePath = documentRoot_ + "/index.html";
    else
      filePath = documentRoot_ + request.path;
    HTTPResponse response;
    if (fs_.exists(filePath))
      response.body = fs_.readFile(filePath);
    else {
      response.statusCode = 404;
      response.reasonPhrase = "Not Found";
      response.body = "<h1>404 Not Found</h1>";
      Logger::warning("File not found: " + filePath);
    }

    // response.headers["Content-Type"]="text/html";
    response.headers["Content-Type"] = MimeResolver::getMime(filePath);
    response.headers["Content-Length"] = std::to_string(response.body.size());
    response.headers["Connection"]="close";
    //   response.headers["Connection"] = "close";
    std::string rawResponse = response.serialize();
    if(client.send(rawResponse.c_str(), rawResponse.size())<0){
      Logger::error("Failed to send response");
      return;
    }

    requestBuffer.clear(); 
    //   if(request.headers["Connection"] == "close")
    //   response.headers["Connection"]="close";
    // else
    // response.headers["Connection"]="keep-alive";
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    Logger::access(request, response, duration.count());
    std::cout << buffer << "\n";
    for (const auto &header : request.headers)
      std::cout << header.first << "<" << header.second << "\n";
    // }
    break;
  }
}
