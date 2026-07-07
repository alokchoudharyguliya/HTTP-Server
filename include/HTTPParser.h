#pragma once
#include "HTTPRequest.h"
class HTTPParser{
	public:
		HTTPRequest parse(const std::string&rawRequest);
};