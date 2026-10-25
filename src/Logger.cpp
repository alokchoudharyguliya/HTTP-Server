#include "Logger.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
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
                    long long latencyMs, size_t bytesSent) { // was earlier log
  std::lock_guard<std::mutex> lock(Logger::mutex_);
  std::cerr << prefix("ACCESS") << request.method << " " << request.path << "->"
            << response.statusCode << "\t"
            // << " | " << response.body.size() << " B "
            << " | " << bytesSent << " B "
            << " | " << latencyMs << "\n";
}
void Logger::info(const std::string &message) {
  if (!enabled(Level::info))
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << prefix("INFO") << message << "\n";
}
void Logger::warning(const std::string &message) {
  if (!enabled(Level::warning))
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << prefix("WARNING") << message << "\n";
}
void Logger::debug(const std::string &message) {
  if (!enabled(Level::debug))
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << prefix("DEBUG") << message << "\n";
}
std::string Logger::prefix(const std::string &pref) {
  return "[" + Logger::currentTimestamp() + "] " + "[" + pref + "] ";
}
void Logger::error(const std::string &message) {
  if (!enabled(Level::error))
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << prefix("ERROR") << message << "\n";
}
Logger::Level Logger::level_ = Logger::Level::info;
void Logger::setLevel(Level level) { level_ = level; }
bool Logger::enabled(Level level) { return level >= level_; }

void Logger::setLevelFromString(const std::string &level) {
  if (level == "debug")
    setLevel(Level::debug);
  else if (level == "info")
    setLevel(Level::info);
  else if (level == "warning")
    setLevel(Level::warning);
  else if (level == "error")
    setLevel(Level::error);
  else
    warning("Unknown log level '" + level + "', using info");
}
