// 业务模块
#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include "json.hpp"
#include "public.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
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
    MsgHandler getHandler(int msg_id);                                          // 获取消息对应的业务处理器
    void clientCloseException( const TcpConnectionPtr &conn);                   // 处理客户端异常退出 -- 没有更改用户登录状态，用户再次登录就无法成功
    void reset();                                                               // 服务器异常退出，重置业务方法（比如使所有用户状态下线）
                    // 不同业务消息对应的处理函数
    void login( const TcpConnectionPtr &conn, json &js, Timestamp time);        // 处理登录业务
    void regist( const TcpConnectionPtr &conn, json &js, Timestamp time);       // 处理注册业务
    void oneChat( const TcpConnectionPtr &conn, json &js, Timestamp time);      // 处理一对一聊天业务
    void addFriend( const TcpConnectionPtr &conn, json &js, Timestamp time);    // 处理添加好友业务
    void createGroup( const TcpConnectionPtr &conn, json &js, Timestamp time);  // 创建群组业务
    void addGroup( const TcpConnectionPtr &conn, json &js, Timestamp time);     // 加入群组业务
    void groupChat( const TcpConnectionPtr &conn, json &js, Timestamp time);    // 群组聊天业务

private:
    ChatService();  // 单例 -- 构造函数私有化
    ChatService(const ChatService &service);  // 单例 -- 拷贝构造私有化
    ~ChatService() = default;  // 单例 -- 析构函数私有化 -- 保护单例的生命周期：在单例模式中，通常单例的创建和销毁都是由单例类自己来管理的（一般伴随程序的生命周期）。将析构函数私有化可以确保单例对象不会在不恰当的时候被外部代码销毁（比如外部调用 delete），从而保证单例的稳定性和可用性。

private:
    std::unordered_map< int, MsgHandler> msg_handler_map;           // 一个消息类型映射一个业务处理函数
    std::unordered_map< int, TcpConnectionPtr> user_conn_map;       // 存储在线用户的通信连接，它的操作一定要注意线程安全！比如多个用户同时登录可能会同时更改 map表的内存，造成数据竟态。用户下线操作也是
    std::mutex map_mtx;                                             // 保证 user_conn_map的线程安全
                    // 业务封装数据操作类接口对象
    UserModel user_model;                                           // user表数据操作类
    OfflineMsgModel offline_msg_model;                              // offline_message表数据操作类
    FriendModel friend_model;                                       // friend表相关业务数据操作接口
    GroupModel group_model;                                         // group相关表相关业务的数据操作接口
};


#endif