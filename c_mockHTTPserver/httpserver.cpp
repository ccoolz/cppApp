#include "httpserver.h"
#include "dealclient.h"

HttpServer::HttpServer()
	: is_stop(false)
{
}

HttpServer::~HttpServer()
{
}

bool HttpServer::Start(unsigned short port_)
{
	// 创建HTTP服务器，绑定端口
	server.createSocket();
	bool bind_res = server.bindPort(port_);
	if( bind_res == false) return false;

	// 服务器线程函数处理服务器事物		（模型: 一个服务器可能有多个服务器[多个服务器线程]，一个服务器线程又可能有多个连接[多个连接通信线程]）
	std::thread thr(&HttpServer::threadFunc, this);
	thr.detach();
	return true;
}

void HttpServer::Stop()
{
	is_stop = true;
}

void HttpServer::threadFunc()
{
	while (is_stop == false)
	{
		XTcp client = server.acceptRequst();
		DealClient* deal_client = new DealClient;		// 控制声明周期: 让DealClient的线程函数处理完后delete this自己删掉
		deal_client->Start(client);
	}
}
