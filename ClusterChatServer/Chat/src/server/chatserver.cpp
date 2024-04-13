#include "chatserver.hpp"
#include "chatservice.hpp"
#include "json.hpp"
#include <iostream>
using namespace std::placeholders;
using json = nlohmann::json;


ChatServer::ChatServer( EventLoop *loop_, const InetAddress &listenAddr, const std::string &nameArg)
        : server(loop_, listenAddr, nameArg)
        , loop(loop_) 
{
    server.setConnectionCallback( std::bind( &ChatServer::onConnection, this, _1));         // 注册连接事件相关回调
    server.setMessageCallback (std::bind( &ChatServer::onMessage, this, _1, _2, _3));       // 注册读写事件相关回调
    server.setThreadNum(4);                                                                 // 设定线程数量
}

void ChatServer::start() 
{
    server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected() == true)                                              // 连接成功返回 true，连接失败、连接断开返回 false
    {
        std::cout 
            << "[ " <<conn->peerAddress().toIpPort() << " -> " 
            << conn->localAddress().toIpPort() << " ] connection established\n";
    }
    else
    {
        std::cout 
            << "[ " << conn->peerAddress().toIpPort() << " -> " 
            << conn->localAddress().toIpPort() << " ] connection break\n";
        conn->shutdown();                                                       // 断连回收资源 { epoll(delete conn_fd) & close(conn_fd) }
    }
}

// 过程的简单描述: 网络模块的 epoll监听到读写事件 -> 调用我们写的回调解析收到的消息 -> 通过 map调用业务类中对应的处理函数
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf_, Timestamp time)
{
    std::string buf = buf_->retrieveAllAsString();      // 缓冲区收到的数据转化为字符串
    json js = json::parse(buf);                         // 数据的反序列化

    // 我们希望通过解析反序列化得到的 json对象的 js[msg_id]获取对应业务的 业务处理器(MsgHandler)，而业务处理器在业务模块定义
    // 根据解析出的需求，网络模块可以通过我们定义的消息处理函数触发对应动作，并传递业务可能所需的 conn json time
    // 以此达到完全解耦网络模块和业务模块代码的目的
    // java中实现解耦往往会使用接口，C++没有接口的概念，但处理方式差不多，一般使用抽象基类完成解耦，我们定义一个 ChatService类实现业务代码
    auto service = ChatService::getInstance();
    auto msgHandler = service->getHandler( js["msgid"].get<int>());            // 虽然在 json中以字符串形式存储，但可以通过get<type>获取指定类型的键值
    msgHandler(conn, js, time);
    // 如上。网络模块中的三行代码就解决了所有的消息对应的业务函数
    // 无需针对不同业务在网络代码中进行相应处理或后期添加业务处理函数时在网络模块增加相应代码，实现了网络和业务的解耦
    // 没有运用多态，也就是基类指向派生类对象的方法实现解耦，不过也用到了抽象基类
    // 在业务抽象基类中的构造函数中，将不同的消息类型与对应的业务处理函数注册到了一个成员 map中形成了对应关系
    // 通过业务类对象提供的唯一实例的 getHandler( int msg_id)方法就可以获取消息类型对应的处理函数，进行业务处理
}
