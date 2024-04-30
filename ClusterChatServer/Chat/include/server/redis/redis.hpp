#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <string>
#include <functional>

class Redis
{
public:
    Redis();
    ~Redis();

    // 连接 redis服务器
    bool connect();

    // 向指定的通道发布消息
    bool publish(int channel, std::string message);

    // 订阅指定的通道发布的消息
    bool subscribe(int channel);

    // 取消订阅过的通道
    bool unsubscribe(int channel);

    // 在独立线程中接收订阅中的通道上发布的消息
    void observer_channel_message();

    // 初始化被 notify后向业务层上报消息的回调函数
    void init_notify_handler( std::function<void(int, std::string)> fn);

private:
    // 负责发布消息的 redis上下文对象
    redisContext *publish_context;

    // 负责订阅相关的 redis上下文对象
    redisContext *subscribe_context;

    // 服务器被通知有订阅通道发布的消息后给 service层上报的的回调函数
    std::function<void (int, std::string)> notify_message_handler;
};

#endif