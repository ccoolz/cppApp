#include "httpserver.h"
#ifndef _WIN32
#include <signal.h>
#endif

int main(int argc, char* argv[])
{
#ifndef _WIN32						// ���� Linux ���ܳ��ֵ��ź����⣬�źſ���ʹ�������
	signal( SIGPIPE, SIG_IGN);
#endif
	unsigned short port = 8080;
	if (argc > 1) port = atoi(argv[1]);
	HttpServer server;
	server.Start(port);


	getchar();
	return 0;
}
