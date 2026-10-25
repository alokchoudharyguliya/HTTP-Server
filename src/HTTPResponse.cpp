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
std::string HTTPResponse::formatChunk(const std::string&data){
  std::ostringstream out;
  out<<std::hex<<data.size()<<"\r\n"<<data<<"\r\n";
  return out.str();
}
std::string HTTPResponse::chunkTerminator(){
  return "0\r\n\r\n";
}