// 这个程序我们实现一个 基于http协议的服务器
// 首先我们要将 xtcp类中的 TcpThread类修改为 HttpThread类
#include "xtcp.h"
int main(int argc, char* argv[])
{
	unsigned short port = 8080;
	if (argc > 1) port = atoi(argv[1]);
	XTcp server;
	server.createSocket();
	bool bind_res = server.bindPort(port);
	if (bind_res == false) return 0;
	for (;;)
	{
		// 接收一个http客户端请求
		XTcp client = server.acceptRequst();

		// 连接socket传给http线程类，连接后的操作交给http线程类的线程函数处理
		HttpThread* tcp_thr = new HttpThread;
		tcp_thr->setClient(client);
		std::thread thr(&HttpThread::threadFunc, tcp_thr);
		thr.detach();
	}
	server.closeSock();
	getchar();
	return 0;
}
