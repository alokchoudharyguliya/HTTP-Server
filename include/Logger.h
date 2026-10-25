#pragma once
#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include <mutex>
#include <string>
class Logger {
  enum class Level { debug, info, warning, error };

private:
  static std::mutex
      mutex_; // this will protect the console, and not the logger object
  static std::string currentTimestamp();
  static std::string prefix(const std::string &pref);
  static Level level_;
  static bool enabled(Level level);

public:
  //   static void log(const HTTPRequest &request, const HTTPResponse &response,
  //   long long latencyMs);
  static void info(const std::string &message);
  static void debug(const std::string &message);
  static void warning(const std::string &message);
  static void error(const std::string &message);
  static void access(const HTTPRequest &request, const HTTPResponse &response,
                     long long latencyMs, size_t bytesSent); // renamed log to access
  static void setLevel(Level level);
  static void setLevelFromString(const std::string &level);
};