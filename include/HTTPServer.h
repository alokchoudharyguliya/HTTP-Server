#pragma once
#include"FileServer.h"
#include"HTTPParser.h"
#include"ThreadPool.h"
#include"Socket.h"
class HTTPServer{
	private:
		FileServer fs_;
		HTTPParser parser_;
		int port_;
		Socket listener_;
		std::string documentRoot_;
		ThreadPool threadPool_;
	public:
	HTTPServer(int port,const std::string&root);
	void handleClient(Socket client);
	void start();
};