// 业务模块
#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include "json.hpp"
#include "public.hpp"
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;
// 消息对应的处理函数的类型
using MsgHandler = std::function<void (const TcpConnectionPtr&, json&, Timestamp)>;


// 聊天服务器业务类                                                              因为一个对象就够了，所以采用单例模式
class ChatService
{
public:
    static ChatService* getInstance();  // 单例 -- 静态方法获取指向唯一单例对象内存的指针 -- 获取指针是因为对象可能较占内存，而指针固定 int大小
    MsgHandler getHandler(int msg_id);                                      // 获取消息对应的业务处理器
    void login( const TcpConnectionPtr &conn, json &js, Timestamp time);        // 处理登录业务
    void regist( const TcpConnectionPtr &conn, json &js, Timestamp time);       // 处理注册业务

private:
    ChatService();  // 单例 -- 构造函数私有化
    ChatService(const ChatService &service);  // 单例 -- 拷贝构造私有化
    ~ChatService() = default;  // 单例 -- 析构函数私有化 -- 保护单例的生命周期：在单例模式中，通常单例的创建和销毁都是由单例类自己来管理的（一般伴随程序的生命周期）。将析构函数私有化可以确保单例对象不会在不恰当的时候被外部代码销毁（比如外部调用 delete），从而保证单例的稳定性和可用性。

private:
    std::unordered_map< int, MsgHandler> msg_handler_map;                   // 一个消息类型映射一个业务处理函数
};


#endif