#include "HTTPServer.h"
#include "HTTPResponse.h"
#include "Logger.h"
#include "MimeResolver.h"
#include <chrono>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <utility>
#include <csignal>

extern volatile sig_atomic_t g_stop;
extern int g_listenFd;
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
  g_listenFd=listener_.fd();
  
  // while (true) {
  while(!g_stop){
    // std::cout << "Waiting for a client\n";
    // Logger::debug("Waiting for a client");
    Socket client = listener_.accept();
    // std::cout << "Client Connected\n";
    if(!client.isValid()||g_stop)
    break;
    Logger::debug("Client Connected");
    // std::thread(&HTTPServer::handleClient,this,std::move(client)).detach();
    client.setReceiveTimeout(2);
    Task task([this, client = std::move(client)]() mutable {
      handleClient(std::move(client));
    });
    threadPool_.submit(std::move(task));
  }
  listener_.close();
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
    bool shouldClose = false;
    auto start = std::chrono::high_resolution_clock::now();
    ssize_t headerEnd = requestBuffer.find("\r\n\r\n");
    while (headerEnd == std::string::npos) // while (request not complete) {
    {
      // ssize_t bytesReceived = client.receive(buffer, sizeof(buffer) - 1);
      // if (!contains "\r\n\r\n") {
      // recv() append into requestBuffer;
      ssize_t bytesReceived = client.receive(buffer, sizeof(buffer) - 1);
      // } else {
      // if (bytesReceived <= 0) {
      //   Logger::debug("Client Disconnected");
      //   return;
      // }

      if (bytesReceived < 0) {
        if ((errno == EAGAIN || errno == EWOULDBLOCK) && !g_stop)
          continue;
        Logger::debug("Client Disconnected");
        return;
      }
      if (bytesReceived == 0) {
        Logger::debug("Client Disconnected");
        return;
      }
      buffer[bytesReceived] = '\0';
      requestBuffer.append(buffer, bytesReceived);
      // parse();
      // HTTPRequest request = parser_.parse(buffer);
      headerEnd = requestBuffer.find("\r\n\r\n");
    }
    const size_t consumed = static_cast<size_t>(headerEnd) + 4;
    HTTPRequest request = parser_.parse(requestBuffer.substr(0, consumed));
    requestBuffer.erase(0, consumed);
    Logger::debug("Request: " + request.method + " " + request.path + " " +
                  request.version);

    // std::cout << ">>Method:" << request.method << "\n";
    // std::cout << ">>Path:" << request.path << "\n";
    // std::cout << ">>Version:" << request.version << "\n";

    Logger::debug("Received request");
    std::string filePath;
    if (request.path == "/")
      filePath = documentRoot_ + "/index.html";
    else
      filePath = documentRoot_ + request.path;
    HTTPResponse response;
    bool streamFile = false;
    std::ifstream file;
    if (fs_.exists(filePath)) {
      // response.body = fs_.readFile(filePath);
      file = fs_.openBinary(filePath);
      streamFile = file.is_open();
    }
    if (!streamFile) {
      response.statusCode = 404;
      response.reasonPhrase = "Not Found";
      response.body = "<h1>404 Not Found</h1>";
      Logger::warning("File not found: " + filePath);
    }

    // response.headers["Content-Type"]="text/html";
    response.headers["Content-Type"] = MimeResolver::getMime(filePath);
    // response.headers["Content-Length"] = std::to_string(response.body.size());
    std::string connection = request.headers["Connection"];
    if (connection.find("close") != std::string::npos)
      shouldClose = true;
    else if (request.version == "HTTP/1.0" &&
             connection.find("keep-alive") == std::string::npos)
      shouldClose = true;
    if (shouldClose) {
      response.headers["Connection"] = "close";
    } else
      response.headers["Connection"] = "keep-alive";
    //   response.headers["Connection"] = "close";
    if (!streamFile) {
      response.headers["Content-Length"] = std::to_string(response.body.size());
      std::string rawResponse = response.serialize();
      if (client.send(rawResponse.c_str(), rawResponse.size()) < 0) {
        Logger::error("Failed to send response");
        return;
      }
    } else {
      response.headers["Transfer-Encoding"] = "chunked";
      std::string rawHeaders = response.serializeHeaders();
      if (client.send(rawHeaders.c_str(), rawHeaders.size()) < 0) {
        Logger::error("Failed to send response");
        return;
      }
      char fileBuffer[4096];
      while (file) {
        file.read(fileBuffer, sizeof(fileBuffer));
        std::streamsize n = file.gcount();
        if (n <= 0)
          break;
        std::ostringstream sizeLine;
        sizeLine << std::hex << n << "\r\n";
        std::string sizeHeader = sizeLine.str();
        if (client.send(sizeHeader.c_str(), sizeHeader.size()) < 0 ||
            client.send(fileBuffer, static_cast<size_t>(n)) < 0) {
          Logger::error("Failed to send response");
          return;
        }
        const char crlf[] = "\r\n";
        if (client.send(crlf, 2) < 0) {
          Logger::error("Failed to send response");
          return;
        }
      }
      const char terminator[] = "0\r\n\r\n";
      if (client.send(terminator, 5) < 0) {
        Logger::error("Failed to send response");
        return;
      }
    }

    // std::string rawResponse = response.serialize();
    // if (client.send(rawResponse.c_str(), rawResponse.size()) < 0) {
    //   Logger::error("Failed to send response");
    //   return;
    // }

    // requestBuffer.clear(); // Don't wipe the buffer, drops bytes that already
    // arrived
    //   if(request.headers["Connection"] == "close")
    //   response.headers["Connection"]="close";
    // else
    // response.headers["Connection"]="keep-alive";
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    Logger::access(request, response, duration.count());
    Logger::debug(std::string("Raw buffer: ") + buffer);
    for (const auto &header : request.headers)
      std::cout << header.first << "<" << header.second << "\n";
    if (shouldClose)
      break;
    // }
  }
}
