#include "chatservice.hpp"
#include "public.hpp"
#include <string>
#include <muduo/base/Logging.h>             // muduo的日志相关
using namespace muduo;


// 网络模块和业务模块解耦的核心: 把每种消息及对应的业务函数都登记到唯一 ChatService对象的成员 map上，可以通过 map反映它们间的映射关系
ChatService::ChatService()
{
    msg_handler_map.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    msg_handler_map.insert({REGIST_MSG, std::bind(&ChatService::regist, this, _1, _2, _3)});
}

ChatService* ChatService::getInstance()
{
    static ChatService service;             // 静态变量只会在静态区初始化一次，后面再次获取该对象就是获取一个已经存在的对象了
    return &service;
}

MsgHandler ChatService::getHandler(int msg_id)
{
    // 出错返回默认处理器，功能是用 muduo库的日志系统记录错误日志: 消息没有对应的事件处理函数。若直接不返回值会导致程序挂掉，不太好
    auto iter = msg_handler_map.find(msg_id);
    if (iter == msg_handler_map.end())
    {
        return [=](const TcpConnectionPtr&, json&, Timestamp) {
            LOG_ERROR << "[msgid]:" << msg_id << " can not find a handler!";
        };                              
    }

    return iter->second;                    // or return ..map[msg_id]
}

void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "execute login...";
}

void ChatService::regist(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "execute regist...";
}
