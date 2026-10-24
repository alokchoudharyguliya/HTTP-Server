#include "FileServer.h"
#include "Logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>

bool FileServer::exists(const std::string &path) {
  return std::filesystem::exists(path);
}
std::string FileServer::readFile(const std::string &path) {
  std::ifstream file(path);
  // if(!file)
  // return "";

  if (!file)
    Logger::warning("Could not open file: " + path);
  std::ostringstream buffer;
  buffer << file.rdbuf(); // provided by STL, trick copy the entire file into a
                          // string
  return buffer.str();
}
std::ifstream FileServer::openBinary(const std::string&path){
  return std::ifstream(path, std::ios::binary);
}