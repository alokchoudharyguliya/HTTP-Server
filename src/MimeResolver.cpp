#include"MimeResolver.h"
// class MimeResolver{
// 	public:
// 		static std::string getMime(const std::string&path);
// };
std::string MimeResolver::getMime(const std::string&path){
	// return "application/octet-stream";
	// return "text/html";
	if(path.ends_with(".html"))
		return "text/html";
	if(path.ends_with(".css"))
		return "text/css";
	if(path.ends_with(".js"))
		return "text/javascript";
	if(path.ends_with(".jpeg"))
		return "image/jpeg";
	if(path.ends_with(".png"))
		return "image/png";
	if(path.ends_with(".jpg"))
		return "image/jpeg";
	if(path.ends_with(".txt"))
		return "text/plain";
	return "application/octet-stream";
	
	
}