#include "chatservice.hpp"
#include "public.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <muduo/base/Logging.h>             // muduo的日志相关
using namespace muduo;


// 网络模块和业务模块解耦的核心: 把每种消息及对应的业务函数都登记到唯一 ChatService对象的成员 map上，可以通过 map反映它们间的映射关系
ChatService::ChatService()
{
    msg_handler_map.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    msg_handler_map.insert({LOGOUT_MSG, std::bind(&ChatService::logout, this, _1, _2, _3)});
    msg_handler_map.insert({REGIST_MSG, std::bind(&ChatService::regist, this, _1, _2, _3)});
    msg_handler_map.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    msg_handler_map.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    msg_handler_map.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    msg_handler_map.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    msg_handler_map.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接 redis服务器
    if (redis.connect())
    {
        // 设置 Redis上报消息函数的具体实现，使上报消息函数满足服务器的具体要求
        redis.init_notify_handler( std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
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

// 处理客户端异常退出 -- 没有更改用户登录状态，用户再次登录就无法成功
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    int id = -1;
    {
        std::lock_guard<std::mutex> lock(map_mtx);          // 维护成员 user_conn_map的线程安全

        for (auto iter = user_conn_map.begin(); iter != user_conn_map.end(); iter++)
        {
            // 1.根据 conn通过 user_conn_map找到对应的 用户id            find()只能通过键找值
            if (iter->second == conn)
            {
                // 2.将用户在数据库中的登录状态改为 offline
                id = iter->first;
                
                // 3.将连接信息从 用户-连接map表中删除
                user_conn_map.erase(iter);
                break;
            }
        }
    }

    // 异常退出，用户下线，取消订阅 id映射的频道
    redis.unsubscribe(id);

    if (id != -1)                                           // 客户端正常退出时 id已被移除，map中找不到，仍为 -1，无需再次操作
    {
        User user = user_model.query(id);                   // 这部分操作本该在2中完成，但为了减小锁的粒度，移到锁的作用域外完成
        user.setState("offline");       
        user_model.updateState(user);
    }
}

// 服务器异常退出，重置业务方法（比如使所有用户状态下线）
void ChatService::reset()
{
    // 把 online用户的 state重置为 offline
    user_model.resetState();
}

// 处理登录业务 -- 收到客户端发送的json字符串，服务器解析消息类型为登录，反序列化 id password
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    std::string password = js["password"];

    User user = user_model.query(id);
    json response;
    if (user.getId() != -1 && password == user.getPassword())     
    {                                       // 若没找到或有错误会返回 User()对象，id = -1
        if (user.getState() == "online")
        {                                   // 虽然查到了，但用户已登录，不能重复登录
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 1;
            response["errmsg"] = "this account is already online, do not log in again!";
            conn->send(response.dump());
        }
        else                                // 登录成功，记录用户连接信息，更新用户状态为 online
        {
            {
                std::lock_guard<std::mutex> lock(map_mtx);
                user_conn_map.insert({id, conn});
            }
            user.setState("online");
            user_model.updateState(user);

            // 用户登录成功，向 Redis订阅 channel(id)
            redis.subscribe(id);

            // 登录回应消息
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            // 若该用户有离线消息，将离线时收到的消息也转发过去
            std::vector<std::string> msgs_vec = offline_msg_model.query(id);
            if (msgs_vec.empty() == false)  // 消息 vector不为空，说明没有离线消息
            {
                response["offlinemsg"] = msgs_vec;
                // 转发完该用户的离线消息后，把这些离线消息从表中清除
                offline_msg_model.remove(id);
            }            

            // 查询用户的好友信息一并发送
            std::vector<User> usrs_vec = friend_model.query(id);
            if (usrs_vec.empty() == false)
            {                               // vector<User> 包含自定义类型，无法直接装到 json，正好好友的 password字段为空无意义，我们可以用一个字符串 vector，每一条 string 代表一个装好友信息的 json对象反序列化的字符串
                std::vector<std::string> friends;
                for (User &user : usrs_vec)
                {
                    json usr_js;
                    usr_js["id"] = user.getId();
                    usr_js["name"] = user.getName();
                    usr_js["state"] = user.getState();
                    friends.push_back(usr_js.dump());
                }
                response["friends"] = friends;
            }
            
            // 查询用户所在的群组信息一并发送
            std::vector<Group> groups = group_model.queryGroups(id);
            if (groups.empty() == false)
            {
                std::vector<std::string> groups_str_vec;
                for (Group &group : groups)
                {
                    json group_js;
                    group_js["id"] = group.getId();
                    group_js["groupname"] = group.getName();
                    group_js["groupdesc"] = group.getDesc();

                    std::vector<GroupUser> group_users = group.getUsers();
                    std::vector<std::string> members_str_vec;
                    for (GroupUser &mem : group_users)
                    {
                        json mem_js;
                        mem_js["id"] = mem.getId();
                        mem_js["name"] = mem.getName();
                        mem_js["state"] = mem.getState();
                        mem_js["role"] = mem.getRole();
                        members_str_vec.push_back(mem_js.dump());       // 自定义类型的 vec无法被装进 json（如 json["users"] = std::vector<GroupUser>无法识别），所以我们以序列化后的 json字符串形式存储，可以被 json识别
                    }
                    group_js["users"] = members_str_vec;
                    groups_str_vec.push_back(group_js.dump());
                }
                response["groups"] = groups_str_vec;
            }

            // 发送所有消息给用户客户端
            conn->send(response.dump());
            // 连续发送两次的目的是解决第一次可能被客户端子线程接收函数读走导致主线程登录函数读不到的情况
            // 设置一定的发送间隔以免被同时读取，同时读取会导致 json解析错误
            std::this_thread::sleep_for( std::chrono::microseconds(500));
            conn->send(response.dump());
        } 
    }
    else                                    // 登录失败
    {
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 2;
        response["errmsg"] = "the user id does not exist or the password is incorrect!";
        conn->send(response.dump());
    }
}

// 处理登出业务
void ChatService::logout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 从用户-连接表中删除登出用户的连接
    int id = js["id"].get<int>();
    {
        std::lock_guard<std::mutex> lock(map_mtx);
        auto iter = user_conn_map.find(id);
        if (iter != user_conn_map.end())
        {
            user_conn_map.erase(iter);
        }
    }

    // 用户下线，从 Redis中取消订阅
    redis.unsubscribe(id);

    // 更新用户状态为离线
    User user(id, "", "", "offline");
    user_model.updateState(user);
}

// 处理注册业务 -- 收到客户端发送的json字符串，服务器解析消息类型为登录，反序列化 name password | id自动生成 state默认offline 不需要
void ChatService::regist(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    std::string name = js["name"];
    std::string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPassword(pwd);

    bool result = user_model.insert(user);
    json response;
    if (result == true)             // 注册成功 -- 回复响应消息
    {
        response["msgid"] = REGIST_MSG_ACK;
        response["errno"] = 0;                      // 我们约定错误码 0为注册成功
        response["id"] = user.getId();              // 把生成的 id也回复给客户端
        conn->send(response.dump());
    }
    else                            // 注册失败
    {
        response["msgid"] = REGIST_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 处理一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 在本服务器中查询被发送消息用户的在线状态（通过 user_conn_map）
    int to_id = js["toid"].get<int>();
    {
        std::lock_guard<std::mutex> lock(map_mtx);
        auto iter = user_conn_map.find(to_id);
        if (iter != user_conn_map.end())       // 连接表中找到了说明对方在线
        {
            // to_id在线，转发消息 -- 为什么要在上锁区间里做，因为转发消息需要对方用户的连接 conn，也就是需要 map获取 conn，若不对这段操作加锁，可能在发送消息前有其他线程的操作使 conn被移除 user_conn_map表，这个 conn失效了导致这次发送操作失效
            iter->second->send(js.dump());
            return;
        }
    }

    // 引入多服务器负载均衡后，若本服务器的连接表中未找到目标 id，可能有两种情况
    // 1.该用户在其他服务器上登录，而非下线，可以通过查询数据库查到它的 state是 online
    User to_user = user_model.query(to_id);
    if (to_user.getState() == "online")
    {       // 在对方订阅的频道上发布消息，对方服务器通过 notify的回调函数将消息上报业务层
        redis.publish(to_id, js.dump());
        return;
    }
    
    // 2.查询数据库它的状态是 offline，说明该用户确实是离线状态，存储离线消息
    offline_msg_model.insert(to_id, js.dump());
}

// 处理添加好友业务     发起人: id  好友: friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int user_id = js["id"].get<int>();
    int friend_id = js["friendid"].get<int>();

    friend_model.insert(user_id, friend_id);
}

// 创建群组业务     创建人: id  群名: groupname  群描述: groupdesc  | 群 id自动生成
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int user_id = js["id"].get<int>();
    std::string group_name = js["groupname"];
    std::string group_desc = js["groupdesc"];

    Group group(-1, group_name, group_desc);        
    if (group_model.createGroup(group) == true);        // group引用传递做参数，id自动生成后会覆盖掉 -1
    {
        // 若创建群成功，存储创建人的身份信息
        std::string role = "creator";
        group_model.addGroup(user_id, group.getId(), role);
    }
}

// 加入群组业务     加入的用户: id  加入的群: id  |  群身份应指定为 normal
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int user_id = js["id"].get<int>();
    int group_id = js["groupid"].get<int>();
    std::string role = "normal";
    group_model.addGroup(user_id, group_id, role);
}

// 群组聊天业务     发消息用户: id  发消息的群聊: groupid
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int user_id = js["id"].get<int>();
    int group_id = js["groupid"].get<int>();

    // 获取其他群聊成员的 id
    std::vector<int> id_vec = group_model.queryGroupUsers(user_id, group_id);

    // 接下来可能涉及到多线程中对 user_conn_map的同时访问，要上锁。若在循环内上锁，每次循环都要上锁和释放锁，浪费资源
    std::lock_guard<std::mutex> lock(map_mtx);
    for (int id : id_vec)
    {       // 对于同一群聊中的每一个 id
        auto iter = user_conn_map.find(id);
        if (iter != user_conn_map.end())
        {
            // 若 id在本服务器上在线，转发消息
            TcpConnectionPtr conn = iter->second;
            conn->send(js.dump());
        } 
        else 
        {
            User user = user_model.query(id);
            if (user.getState() == "online")
            {
                // 在其他服务器上在线，转发消息
                redis.publish(id, js.dump());
            }
            else
            {
                // 不在线，存储离线消息
                offline_msg_model.insert(id, js.dump());
            }
        }
    }
}

// 从 redis消息队列中获取订阅的消息     服务器业务对象初始化过程中将该函数指针的值赋给 Redis处理订阅消息的回调函数
void ChatService::handleRedisSubscribeMessage(int userid, std::string msg)
{
    {
        std::lock_guard<std::mutex> lock(map_mtx);
        // 可能发生这种情况: 另一台服务器的用户发送消息时本服务器的目标用户还未下线，
        // 但本服务器接收到该消息时或之前该用户下线了，那么需要在服务器接收到上报的消息后判断该用户的在线情况
        auto iter = user_conn_map.find(userid);
        if (iter != user_conn_map.end())
        {
            iter->second->send(msg);
            return;
        }
    }
    offline_msg_model.insert(userid, msg);
}
