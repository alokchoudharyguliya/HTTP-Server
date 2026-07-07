#pragma once
#include<string>
#include<unordered_map>
// OUR DATA CLASS
class HTTPRequest{
	public:
		std::string method;
		std::string path;
		std::string version;
		std::unordered_map<std::string,std::string>headers;
		std::string body;
};
