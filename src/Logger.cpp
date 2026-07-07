#include "Logger.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
std::mutex Logger::mutex_;

std::string Logger::currentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::ostringstream oss;
  oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

  return oss.str();
}

void Logger::access(const HTTPRequest &request, const HTTPResponse &response,
                    long long latencyMs) { // was earlier log
  std::lock_guard<std::mutex> lock(Logger::mutex_);
  std::cerr << prefix("ACCESS") << request.method << " " << request.path << "->"
            << response.statusCode << "\t"
            << " | " << response.body.size() << " B "
            << " | " << latencyMs << "\n";
}
void Logger::info(const std::string &message) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << prefix("INFO") << message << "\n";
}
void Logger::warning(const std::string &message) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << prefix("WARNING") << message << "\n";
}
void Logger::debug(const std::string &message) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << prefix("DEBUG") << message << "\n";
}
std::string Logger::prefix(const std::string &pref) {
  return "[" + Logger::currentTimestamp() + "] " + "[" + pref + "] ";
}
void Logger::error(const std::string &message) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << prefix("ERROR") << message << "\n";
}