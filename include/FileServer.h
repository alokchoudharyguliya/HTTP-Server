#pragma once
#include<string>
#include<fstream>
class FileServer{
    public:
    std::string readFile(const std::string&path);
    std::ifstream  openBinary(const std::string&path);
    bool exists(const std::string&path);
};