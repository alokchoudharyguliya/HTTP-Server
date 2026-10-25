#include "HTTPParser.h"
#include "Logger.h"
#include <algorithm>
#include <iostream>
#include <sstream>

HTTPRequest HTTPParser::parse(const std::string &rawRequest) {
  HTTPRequest request;
  std::istringstream stream(rawRequest);
  stream >> request.method >> request.path >> request.version;
  // stream.ignore();
  if (stream.peek() == '\r')
    stream.ignore();
  if (stream.peek() == '\n')
    stream.ignore();
  std::string line;
  int lineNo = 1;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      break;
    // std::cout<<"Line "<<lineNo++<<":"<<line<<"\n";
    Logger::debug("Header line " + std::to_string(lineNo) + ": " + line);
    std::string::size_type pos = line.find(':');
    if (pos == std::string::npos)
      continue;
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);
    auto start = value.find_first_not_of(" \t");
    auto end = value.find_last_not_of(" \t");
    if (start == std::string::npos)
      value.clear();
    else
      value = value.substr(start, end - start + 1);
    request.headers[key] = value;
  }
  return request;
}
