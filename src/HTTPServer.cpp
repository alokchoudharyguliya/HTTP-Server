#include "HTTPServer.h"
#include "HTTPResponse.h"
#include "Logger.h"
#include "MimeResolver.h"
#include <cerrno>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <filesystem>
#include <sstream>
#include <string>
#include <sys/event.h>
#include <sys/socket.h>
#include <thread>
#include <utility>
#include<unistd.h>

namespace fs = std::filesystem;
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

// Ask kqueue to wake kevent when this descriptor has bytes to read.
// EVFILT_READ stays registered, so a keep-alive socket is watched without a thread blocked in recv.
static void watchRead(int kq, int fd) {
  struct kevent change;
  EV_SET(&change, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
  ::kevent(kq, &change, 1, nullptr, 0, nullptr);
}

void HTTPServer::acceptReady() {
  // One wake can mean several clients are already queued. Keep accepting until
  // the non-blocking socket says there are no more (EAGAIN), which is normal.
  while (!g_stop) {
    Socket client = listener_.accept();
    if (!client.isValid()) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;
      Logger::error("Failed to accept client");
      return;
    }
    client.setNonBlocking();
    int fd = client.fd();
    watchRead(kq_, fd);
    connections_.emplace(fd, Connection{std::move(client), {}});
  }
}

void HTTPServer::readReady(int fd) {
  auto found = connections_.find(fd);
  if (found == connections_.end())
    return;
  Connection &conn = found->second;
  char buffer[4096];
  bool closed = false;
  while (true) {
    ssize_t n = conn.socket.receive(buffer, sizeof(buffer));
    if (n < 0) {
      // EAGAIN means the kernel has nothing more right now. The connection stays registered.
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      connections_.erase(found);
      return;
    }
    if (n == 0) {
      // The peer closed the connection. Bytes already read may still hold a full request.
      closed = true;
      break;
    }
    conn.buffer.append(buffer, static_cast<size_t>(n));
  }
  // auto headerEnd = conn.buffer.find("\r\n\r\n");
  // if (headerEnd == std::string::npos)
  //   return;
  // One recv can contain more than one request. Handle each completed one
  // before returning to kevent, or the leftover request would wait for more bytes.
  while (conn.buffer.find("\r\n\r\n") != std::string::npos) {
    bool shouldClose = handleOneRequest(conn.socket, conn.buffer);
    if (shouldClose) {
      connections_.erase(fd);
      return;
    }
  }
  if (closed)
    connections_.erase(fd);
}

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
  Logger::info("  Event loop    : kqueue, one thread");
  Logger::info("Listening on port " + std::to_string(port_));
  listener_.listen();
  g_listenFd = listener_.fd();

  // The listening socket must not block in accept. kqueue sleeps once for every watched fd.
  listener_.setNonBlocking();
  kq_ = ::kqueue();
  watchRead(kq_, listener_.fd());
  struct kevent events[64];
  // while (true) {
  // while (!g_stop) {
  //   // std::cout << "Waiting for a client\n";
  //   // Logger::debug("Waiting for a client");
  //   Socket client = listener_.accept();
  //   // std::cout << "Client Connected\n";
  //   if (!client.isValid() || g_stop)
  //     break;
  //   Logger::debug("Client Connected");
  //   //
  //   std::thread(&HTTPServer::handleClient,this,std::move(client)).detach();
  //   client.setReceiveTimeout(2);
  //   Task task([this, client = std::move(client)]() mutable {
  //     handleClient(std::move(client));
  //   });
  //   threadPool_.submit(std::move(task));
  // }

  while (!g_stop) {
    // Sleep until any registered socket is readable. EINTR is a signal during that wait.
    int n = ::kevent(kq_, nullptr, 0, events, 64, nullptr);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    for (int i = 0; i < n; i++) {
      int fd = static_cast<int>(events[i].ident);
      if (fd == listener_.fd())
        acceptReady();
      else
        readReady(fd);
    }
  }
  connections_.clear();
  if (kq_ >= 0) {
    ::close(kq_);
    kq_ = -1;
  }
  listener_.close();
}

bool HTTPServer::handleOneRequest(Socket &client, std::string &requestBuffer) {
  // char buffer[4097];
  // std::string requestBuffer;
  // while (true) {
  auto headerEnd = requestBuffer.find("\r\n\r\n");
  if (headerEnd == std::string::npos)
    return false;
  bool shouldClose = false;
  auto start = std::chrono::high_resolution_clock::now();
  const size_t consumed = headerEnd + 4;
  HTTPRequest request = parser_.parse(requestBuffer.substr(0, consumed));
  requestBuffer.erase(0, consumed);
  // ssize_t headerEnd = requestBuffer.find("\r\n\r\n");
  Logger::debug("Request: " + request.method + " " + request.path + " " +
                request.version);

  Logger::debug("Received request");
  std::string filePath;
  if (request.path == "/")
    filePath = documentRoot_ + "/index.html";
  else
    filePath = documentRoot_ + request.path;
  fs::path root = fs::weakly_canonical(documentRoot_);
  fs::path resolved = fs::weakly_canonical(filePath);
  std::string rootText = root.string();
  if (!rootText.empty() && rootText.back() != '/')
    rootText.push_back('/');
  bool insideRoot = resolved == root || resolved.string().starts_with(rootText);

  HTTPResponse response;
  bool streamFile = false;
  std::ifstream file;
  size_t bytesSent = 0;

  if (insideRoot && fs_.exists(filePath)) {
    file = fs_.openBinary(filePath);
    streamFile = file.is_open();
  }
  if (!insideRoot) {
    response.statusCode = 403;
    response.reasonPhrase = "Forbidden";
    response.body = "<h1>403 Forbidden</h1>";
    bytesSent = response.body.size();

    Logger::warning("Rejected path outside the document root: " + filePath);
  } else if (!streamFile) {
    response.statusCode = 404;
    response.reasonPhrase = "Not Found";
    response.body = "<h1>404 Not Found</h1>";
    bytesSent = response.body.size();
    Logger::warning("File not found: " + filePath);
  }
  response.headers["Content-Type"] = MimeResolver::getMime(filePath);
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

  if (!streamFile) {
    response.headers["Content-Length"] = std::to_string(response.body.size());
    std::string rawResponse = response.serialize();
    if (client.send(rawResponse.c_str(), rawResponse.size()) < 0) {
      Logger::error("Failed to send response");
      return true;
    }
  } else {
    response.headers["Transfer-Encoding"] = "chunked";
    std::string rawHeaders = response.serializeHeaders();
    if (client.send(rawHeaders.c_str(), rawHeaders.size()) < 0) {
      Logger::error("Failed to send response");
      return true;
    }
    char fileBuffer[4096];
    while (file) {
      file.read(fileBuffer, sizeof(fileBuffer));
      std::streamsize n = file.gcount();
      if (n <= 0)
        break;
      bytesSent += static_cast<size_t>(n);
      std::string chunk = HTTPResponse::formatChunk(
          std::string(fileBuffer, static_cast<size_t>(n)));
      if (client.send(chunk.c_str(), chunk.size()) < 0) {
        Logger::error("Failed to send response");
        return true;
      }
    }
    std::string terminator = HTTPResponse::chunkTerminator();
    if (client.send(terminator.c_str(), terminator.size()) < 0) {
      Logger::error("Failed to send response");
      return true;
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  Logger::access(request, response, duration.count(), bytesSent);
  // Logger::debug(std::string("Raw buffer: ") + buffer);
  for (const auto &header : request.headers)
    Logger::debug(header.first + "<" + header.second + "\n");
  // if (shouldClose)
  //   break;
  // }
  return shouldClose;
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

// void HTTPServer::handleClient(Socket client) {
//   char buffer[4097];
//   std::string requestBuffer;
//   while (true) {
//     bool shouldClose = false;
//     auto start = std::chrono::high_resolution_clock::now();
//     ssize_t headerEnd = requestBuffer.find("\r\n\r\n");
//     while (headerEnd == std::string::npos) // while (request not complete) {
//     {
//       // ssize_t bytesReceived = client.receive(buffer, sizeof(buffer) - 1);
//       // if (!contains "\r\n\r\n") {
//       // recv() append into requestBuffer;
//       ssize_t bytesReceived = client.receive(buffer, sizeof(buffer) - 1);
//       // } else {
//       // if (bytesReceived <= 0) {
//       //   Logger::debug("Client Disconnected");
//       //   return;
//       // }

//       if (bytesReceived < 0) {
//         if ((errno == EAGAIN || errno == EWOULDBLOCK) && !g_stop)
//           continue;
//         Logger::debug("Client Disconnected");
//         return;
//       }
//       if (bytesReceived == 0) {
//         Logger::debug("Client Disconnected");
//         return;
//       }
//       buffer[bytesReceived] = '\0';
//       requestBuffer.append(buffer, bytesReceived);
//       // parse();
//       // HTTPRequest request = parser_.parse(buffer);
//       headerEnd = requestBuffer.find("\r\n\r\n");
//     }
//     const size_t consumed = static_cast<size_t>(headerEnd) + 4;
//     HTTPRequest request = parser_.parse(requestBuffer.substr(0, consumed));
//     requestBuffer.erase(0, consumed);
//     Logger::debug("Request: " + request.method + " " + request.path + " " +
//                   request.version);

//     // std::cout << ">>Method:" << request.method << "\n";
//     // std::cout << ">>Path:" << request.path << "\n";
//     // std::cout << ">>Version:" << request.version << "\n";

//     Logger::debug("Received request");
//     std::string filePath;
//     if (request.path == "/")
//       filePath = documentRoot_ + "/index.html";
//     else
//       filePath = documentRoot_ + request.path;
//     fs::path root = fs::weakly_canonical(documentRoot_);
//     fs::path resolved = fs::weakly_canonical(filePath);
//     std::string rootText = root.string();
//     if (!rootText.empty() && rootText.back() != '/')
//       rootText.push_back('/');
//     bool insideRoot =
//         resolved == root || resolved.string().starts_with(rootText);

//     HTTPResponse response;
//     bool streamFile = false;
//     std::ifstream file;
//     size_t bytesSent = 0;

//     if (insideRoot && fs_.exists(filePath)) {
//       // response.body = fs_.readFile(filePath);
//       file = fs_.openBinary(filePath);
//       streamFile = file.is_open();
//     }
//     if (!insideRoot) {
//       response.statusCode = 403;
//       response.reasonPhrase = "Forbidden";
//       response.body = "<h1>403 Forbidden</h1>";
//       bytesSent = response.body.size();

//       Logger::warning("Rejected path outside the document root: " +
//       filePath);
//     } else if (!streamFile) {
//       response.statusCode = 404;
//       response.reasonPhrase = "Not Found";
//       response.body = "<h1>404 Not Found</h1>";
//       bytesSent = response.body.size();
//       Logger::warning("File not found: " + filePath);
//     }

//     // response.headers["Content-Type"]="text/html";
//     response.headers["Content-Type"] = MimeResolver::getMime(filePath);
//     // response.headers["Content-Length"] =
//     // std::to_string(response.body.size());
//     std::string connection = request.headers["Connection"];
//     if (connection.find("close") != std::string::npos)
//       shouldClose = true;
//     else if (request.version == "HTTP/1.0" &&
//              connection.find("keep-alive") == std::string::npos)
//       shouldClose = true;
//     if (shouldClose) {
//       response.headers["Connection"] = "close";
//     } else
//       response.headers["Connection"] = "keep-alive";
//     //   response.headers["Connection"] = "close";

//     if (!streamFile) {
//       response.headers["Content-Length"] =
//       std::to_string(response.body.size()); std::string rawResponse =
//       response.serialize(); if (client.send(rawResponse.c_str(),
//       rawResponse.size()) < 0) {
//         Logger::error("Failed to send response");
//         return;
//       }
//     } else {
//       response.headers["Transfer-Encoding"] = "chunked";
//       std::string rawHeaders = response.serializeHeaders();
//       if (client.send(rawHeaders.c_str(), rawHeaders.size()) < 0) {
//         Logger::error("Failed to send response");
//         return;
//       }
//       char fileBuffer[4096];
//       while (file) {
//         file.read(fileBuffer, sizeof(fileBuffer));
//         std::streamsize n = file.gcount();
//         if (n <= 0)
//           break;
//         // std::ostringstream sizeLine;
//         // sizeLine << std::hex << n << "\r\n";
//         // std::string sizeHeader = sizeLine.str();
//         bytesSent += static_cast<size_t>(n);
//         std::string chunk = HTTPResponse::formatChunk(
//             std::string(fileBuffer, static_cast<size_t>(n)));
//         if (client.send(chunk.c_str(), chunk.size()) < 0
//             // || client.send(fileBuffer, static_cast<size_t>(n)) < 0
//         ) {
//           Logger::error("Failed to send response");
//           return;
//         }
//         // const char crlf[] = "\r\n";
//         // if (client.send(crlf, 2) < 0) {
//       }
//       std::string terminator = HTTPResponse::chunkTerminator();
//       if (client.send(terminator.c_str(), terminator.size()) < 0) {
//         Logger::error("Failed to send response");
//         return;
//       }
//       // const char terminator[] = "0\r\n\r\n";
//       // if (client.send(terminator, 5) < 0) {
//       // Logger::error("Failed to send response");
//       // return;
//       // }
//     }

//     // std::string rawResponse = response.serialize();
//     // if (client.send(rawResponse.c_str(), rawResponse.size()) < 0) {
//     //   Logger::error("Failed to send response");
//     //   return;
//     // }

//     // requestBuffer.clear(); // Don't wipe the buffer, drops bytes that
//     already
//     // arrived
//     //   if(request.headers["Connection"] == "close")
//     //   response.headers["Connection"]="close";
//     // else
//     // response.headers["Connection"]="keep-alive";
//     auto end = std::chrono::high_resolution_clock::now();
//     auto duration =
//         std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

//     Logger::access(request, response, duration.count(), bytesSent);
//     Logger::debug(std::string("Raw buffer: ") + buffer);
//     for (const auto &header : request.headers)
//       // std::cout << header.first << "<" << header.second << "\n";
//       Logger::debug(header.first + "<" + header.second + "\n");
//     if (shouldClose)
//       break;
//     // }
//   }
// }