#include "HTTPResponse.h"
#include <sstream>
std::string HTTPResponse::serialize() const {
  std::ostringstream response;
  response << "HTTP/1.1 " << statusCode << " " << reasonPhrase << "\r\n";
  for (const auto &header : headers) {
    response << header.first << ": " << header.second << "\r\n";
  }
  response << "\r\n";
  response << body;
  return response.str();
}
std::string HTTPResponse::serializeHeaders() const {
  std::ostringstream response;
  response<<"HTTP/1.1 "<<statusCode<<" "<<reasonPhrase<<"\r\n";
  for(const auto&header:headers){
	response<<header.first<<": "<<header.second<<"\r\n";
  }
	response<<"\r\n";
	return response.str();
}