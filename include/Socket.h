#pragma once
#include<sys/types.h>
class Socket{
	public:
		Socket():socketFd_(-1){}
		explicit Socket(int fd):socketFd_(fd){}

		~Socket(){
			close();
		}
		Socket(const Socket&)=delete;
		Socket&operator=(const Socket&)=delete;

		Socket(Socket&&other)noexcept;
		Socket&operator=(Socket&&other)noexcept;
		void create();
		void bind(int port);
		void listen(int backlog=10);
		void setReuseAddress(bool enable=true);
		Socket accept();
		ssize_t send(const void*buffer,size_t length);
		ssize_t receive(void*buffer, size_t length);
		void close();
		bool isValid()const;
		int fd() const;
		void setReceiveTimeout(int seconds);
		void setNonBlocking();
	private:
		int socketFd_;
};