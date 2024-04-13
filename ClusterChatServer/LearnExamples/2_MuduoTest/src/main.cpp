/*
muduo网络库给用户提供了两个主要的类
TcpServer
TcpClient
封装了 epoll + 线程池
好处: 能把网路 I/O的代码和业务代码区分开，用户基本只关心两件事  ===  1 用户的连接和断开   2 已建立连接用户的可读写事件
*/
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <functional>
#include <string>
using namespace muduo;
using namespace std::placeholders;
/*
基于 muduo网络库开发服务器程序
    1. 创建 TcpServer对象
    2. 创建 EventLoop事件循环对象的指针  === 一般 TcpServer对象要和 EventLoop对象配合使用
    3. 明确 TcpServer需要什么参数，因为 muduo库源码中它只有一个有参构造函数，所以要在 ChatServer构造函数中构造 TcpServer对象
            TcpServer(EventLoop* loop,
                const InetAddress& listenAddr,
                const string& nameArg,
                Option option = kNoReusePort);
    4. 写连接事件和读写事件的回调函数，并在本服务器类的构造函数中将它们注册到 muduo库的服务器对象中
             Set connection callback.
             Not thread safe.
                    void setConnectionCallback(const ConnectionCallback& cb)
                    { connectionCallback_ = cb; }

             Set message callback.
             Not thread safe.
                    void setMessageCallback(const MessageCallback& cb)
                    { messageCallback_ = cb; }
            
            看看要传入的参数 ConnectionCallback | MessageCallback 是怎样的回调函数类型
                typedef std::function<void (const TcpConnectionPtr&)> ConnectionCallback;   
                     the data has been read to (buf, len)
                typedef std::function<void (const TcpConnectionPtr&, Buffer*, Timestamp)> MessageCallback;

    5. 设置服务器端的线程数量
    6. 本服务器封装 muduo库中的 start()方法开启事件循环
*/
class ChatServer
{
public:
    ChatServer(net::EventLoop* loop_,                                       // 事件循环             #3
               const net::InetAddress& listenAddr,                          // 服务器 IP + port
               const string& nameArg)                                       // 服务器的名字
        : loop(loop_)                                                       // loop_赋给类的成员 loop，这样就可以操作 epoll了
        , server(loop_, listenAddr, nameArg)
    {
        // 给服务器注册[用户连接的创建和断开]回调         #4                   事件发生，让回调自动帮我们处理相应业务，只需在回调中写业务即可。无需关注事件发生的时间点，这些都有 muduo底层帮我们关注
        server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, std::placeholders::_1));                             // 写成成员方法是想访问成员变量，但是成员方法自带一个 this指针，与 CallBack要求传入的一个参数(const TcpConnectionPtr&)的std::function函数传入参数数量不一样。可以通过 bind绑定实现传入成员方法作为参数

        // 给服务器注册[用户读写事件]回调                 #4
        server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

        // 设置服务器端的线程数量                         #5                  若设为4，muduo库会自动划分为 1个I/O线程，3个工作线程。muduo库模型中一个监听主线程，下属多个工作线程，一个线程对应一个eopll，处理一条连接上要监听的事件   若不设定数量，则只有一个线程
        server.setThreadNum(4);                         
    }

    void start()
    {
        server.start();
    }

private:
    void onConnection(const net::TcpConnectionPtr &conn)
    {
        // 连接请求有成功有失败，连接事件也有主动连接和主动关闭
        if (conn->connected() == true)   
        {
            std::cout 
                << conn->peerAddress().toIpPort() << " -> "                 // peerAddress() 返回对端网络地址，toIpPort()返回string类型地址
                << conn->localAddress().toIpPort() << " [state]:online\n";
        }
        else
        {
            std::cout 
                << conn->peerAddress().toIpPort() << " -> "
                << conn->localAddress().toIpPort() << " [state]:offline\n";
            conn->shutdown(); // close(conn_fd)
            // loop.quit(); -> epoll_ctl(delete listen_fd) & close(listen_fd) 相当于退出服务器，回收所有相关资源
        }
    }

    void onMessage(const net::TcpConnectionPtr &conn,                       // 连接        有连接才能收发信息
                   net::Buffer *buf_,                                       // 缓冲区
                   Timestamp time_)                                         // 接收到数据的时间信息
    {
        std::string buf = buf_->retrieveAllAsString();                      // 检索(retrieve)缓冲区对象(muduo::net::Buffer)里的所有数据，并以string形式返回
        std::string time = time_.toString();                                // muduo::Timestamp（时间戳）对象封装了 toString()方法返回 string形式的时间信息数据
        std::cout << "recv data:\n" << buf << "time: " << time << "\n";
        conn->send(buf);                                                    // muduo::net::TcpConnectionPtr类中的 void send(const StringPiece& message);方法中的 muduo::StringPiece类中有传入 string构造函数StringPiece(const string& str): ptr_(str.data()), length_(static_cast<int>(str.size())) { }  这里随便处理一下，将接收到的数据再发回给对端
    }

private:
    net::TcpServer server;  // #1
    net::EventLoop *loop;   // #2 epoll
};


int main()
{
    // 创建服务器对象
    net::EventLoop loop;                                    // 相当于 epoll
    net::InetAddress addr("192.168.115.128", 5000);
    ChatServer chat_server(&loop, addr, "Chat Server");

    // 启动事件循环
    chat_server.start();                                    // 把 listen_fd套接字通过 eopll_ctl注册到eopll上
    loop.loop();                                            // 相当于 eopll_wait()  =>  以阻塞方式等待事件发生，发生自动调用我们注册的事件相应回调函数（新用户连接事件 | 已连接用户的读写事件）

    return 0;
}


/*
    setConnectionCallback函数想传入的参数 -> onConnection(const TcpConnectionPtr&)
    我们实际需要传入的参数 onConnection(this, const TcpConnectionPtr&) 
    placeholders::_1 代表传入原函数的第一个参数，onConnection定义中只有一个参数也是第一个参数 const TcpConnectionPtr&，根据顺序，这个原函数的第一个参数作为绑定函数的第二个参数传入
    那么 (&ChatServer::onConnection, this, std::placeholders::_1)) 相当于 ChatServer::onConnection(this, const TcpConnectionPtr&)传入了setConnectionCallback()函数作为参数 std::function<void (const TcpConnectionPtr&)>
    注意：因为成员方法默认 this作为第一个隐含参数，所以 bind()中除了第一个参数是要绑定的函数指针外，第二个参数必须是 this，这样它才能在调用时作为第一个参数传入
    在 setConnectionCallback函数看来，onConnection函数仍然只有一个参数，符合它的要求，但实际上通过bind绑定函数和参数多传入了我们需要的参数 this
*/