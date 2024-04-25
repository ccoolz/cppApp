// 网络模块
#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <string>
using namespace muduo;
using namespace muduo::net;


class ChatServer
{
public:
    ChatServer( EventLoop *loop_, const InetAddress &listenAddr, const std::string &nameArg);       // 初始化服务器对象
    void start();                                                                                   // 开启服务器的服务

private:
    void onConnection(const TcpConnectionPtr &conn);                                                // 上报连接相关信息的回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf_, Timestamp time_);                    // 上报读写事件相关信息的回调

private:
    TcpServer server;
    EventLoop *loop;    // epoll
};


#endif
