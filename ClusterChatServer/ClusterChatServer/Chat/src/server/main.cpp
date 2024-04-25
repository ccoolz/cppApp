#include "chatserver.hpp"
#include "chatservice.hpp"
#include <signal.h>
#include <iostream>

// 处理服务器 Ctrl + c 结束后，重置用户的状态信息
void resetHandler(int signum)
{
    printf("catch signal %d\n", signum);
    ChatService::getInstance()->reset();
    exit(0);
}

int main(int argc, char *argv[])
{
    // 通过命令行参数解析出服务器 ip和 port
    if (argc < 3)
    {
        std::cerr << "invalid command! example: ./ChatServer 127.0.0.1 5000\n";
        exit(-1);
    }
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 处理服务器 Ctrl + c 结束后，重置用户的状态信息
    signal( SIGINT, resetHandler);

    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();

    return 0;
}

/* 关于 signal(int signum, sighandler_t handler)函数
      在 Linux中,可以按 Ctrl+c键来中止终端中的运行程序，它向正在运行的程序发送 SIGINT 信号以强制退出该命令。
      signal 定义：
            typedef void (*SIGHANDLE)(int)；
            SIGHANDLE signal(int signum, SIGHANDLE handler));
      当接收到一个类型为 signum的信号时，就执行 handler 所指定的函数。（int）signum是传递给它的唯一参数。执行了signal()调用后，进程只要接收到类型为sig的信号，不管其正在执行程序的哪一部分，就立即执行func()函数。当func()函数执行结束后，控制权返回进程被中断的那一点继续执行。 
      总结 ：Ctrl+c 会向程序发送 SIGINT 信号以强制退出，但程序中的 signal(SIGINT,handler)函数收到信号会暂停退出行为，直到执行完 handler(SIGINT)函数
*/ 