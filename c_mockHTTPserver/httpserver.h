#ifndef HTTPSERVER_H
#define HTTPSERVER_H
#include "xtcp.h"

class HttpServer
{
public:
	HttpServer();
	~HttpServer();
	bool Start(unsigned short port_);
	void Stop();
	void threadFunc();

public:
	XTcp server;
	bool is_stop;
};

#endif