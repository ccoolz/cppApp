#include "chatserver.hpp"



int main()
{
    EventLoop loop;
    InetAddress addr("127.0.0.1", 5000);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
}