#pragma once
#include<string>
#include<unordered_map>
class HTTPResponse{
	public:
		int statusCode=200;
		std::string reasonPhrase="OK";
		std::unordered_map<std::string,std::string>headers;
		std::string body;
		std::string serialize() const;
		std::string serializeHeaders() const;
};
