#include "httpserver.h"
#ifndef _WIN32
#include <signal.h>
#endif

int main(int argc, char* argv[])
{
#ifndef _WIN32						// 处理 Linux 可能出现的信号问题，信号可能使程序崩溃
	signal( SIGPIPE, SIG_IGN);
#endif
	unsigned short port = 8080;
	if (argc > 1) port = atoi(argv[1]);
	HttpServer server;
	server.Start(port);


	getchar();
	return 0;
}
