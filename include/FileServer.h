#pragma once
#include<string>
class FileServer{
    public:
    std::string readFile(const std::string&path);
    bool exists(const std::string&path);
};