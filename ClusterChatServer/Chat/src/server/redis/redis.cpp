#include "redis.hpp"
#include <iostream>

// redis对象构造初始化成员指针
Redis::Redis()
    : publish_context(nullptr), subscribe_context(nullptr)
{
}

// redis对象析构释放成员指针
Redis::~Redis()
{
    if (publish_context != nullptr)
    {
        redisFree(publish_context);
    }
    if (subscribe_context != nullptr)
    {
        redisFree(subscribe_context);
    }
}

bool Redis::connect()
{
    // 这条 redis通信连接管理 publish发布消息上下文
    publish_context = redisConnect("127.0.0.1", 6379);  // redis服务器默认工作在 6379端口
    if (publish_context == nullptr)
    {
        std::cerr << "connect redis failed!\n";
        return false;
    }

    // 这条 redis通信连接管理 subscribe订阅消息上下文
    subscribe_context = redisConnect("127.0.0.1", 6379);
    if (subscribe_context == nullptr)
    {
        std::cerr << "connect redis failed!\n";
        return false;
    }

    // 在单独线程中，监听通道上发生的事件，如果有消息就给业务层上报
    std::thread t([&]() {
        observer_channel_message();
    });
    t.detach();

    std::cout << "connect redis server successfully!\n";
    return true;
}

// 向指定的 redis通道发布指定的消息
bool Redis::publish(int channel, std::string message)
{
    redisReply *reply = (redisReply*)redisCommand(publish_context, "PUBLISH %d %s", channel, message.c_str());
                        // 参数：在哪个上下文，执行哪一条命令(format)
    if (reply = nullptr)
    {
        std::cerr << "publish command failed!\n";
        return false; 
    }
    
    freeReplyObject(reply);
    return true;    
}

// 订阅指定的通道，以收到在该通道上发布的消息
bool Redis::subscribe(int channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里发生消息，因此这里仅订阅通道，不接受通道消息
    // 接收通道消息专门由 redis连接成功后新开的线程中的线程函数 observer_channel_message去做
    if (REDIS_ERR == redisAppendCommand(subscribe_context, "SUBSCRIBE %d", channel))
    {                   // redisAppendCommand将命令添加到缓冲区
        std::cerr << "subscribe command failed!\n";
        return false;
    }

    // 只发送命令，不阻塞接收 redis server的消息，否则会和 notifyMsg线程抢占响应资源
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕 (done被置为 1)
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(subscribe_context, &done))
        {               // 循环确保命令被完全发送
            std::cerr << "subscribe command failed!\n";
            return false;
        }
    }
    // redisGetReply 不进行接收响应

    return true;
}

// 取消订阅的 redis通道以停止接收该通道上发布的消息
bool Redis::unsubscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(subscribe_context, "UNSUBSCRIBE %d", channel))
    {
        std::cerr << "unsubscribe command failed!\n";
        return false;
    }

    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(subscribe_context, &done))
        {
            std::cerr << "unsubscribe command failed!\n";
            return false;
        }
    }

    return true;
}

// 在独立线程中接收在订阅通道中发布的消息
void Redis::observer_channel_message()
{
    // 定义一个指向redisReply结构体的指针，用于存储从Redis服务器接收到的回复。
    redisReply *reply = nullptr;
    // 使用redisGetReply函数在一个循环中从Redis订阅上下文中获取回复。
    // 这个循环会持续进行，直到redisGetReply返回非REDIS_OK的值，通常是因为连接被关闭或出现了错误。
    // 如果函数返回 REDIS_OK，那么这通常意味着操作已经成功完成。对于 redisGetReply 函数来说，
    // 返回 REDIS_OK 表示已经成功地从 Redis 服务器接收到了一个回复，并且该回复可以通过函数提供的参数来访问。
    // 请注意，即使 redisGetReply 返回了 REDIS_OK，你也应该检查返回的 redisReply 结构体是否有效，
    // 以及它是否包含了你所期望的数据。例如，在订阅/发布模式下，你可能期望接收到的回复是一个包含频道名和消息内容的数组。
    // 你应该检查回复的类型和内容，以确保它们是正确的。另外，需要注意的是，redisGetReply 函数在接收到回复后会立即返回，
    // 但这并不意味着 Redis 连接已经关闭或没有更多的回复可用。在订阅/发布模式下，
    // 你可以继续调用 redisGetReply 来接收后续的消息，直到 Redis 连接关闭或出现错误。也就是这里的 while

    while (REDIS_OK == redisGetReply(subscribe_context, (void**)&reply))
    {
        // 收到的是一个带三元素的数组
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 向业务层上报通道上发布的消息
            notify_message_handler( atoi(reply->element[1]->str), reply->element[2]->str);
        }

        freeReplyObject(reply);
    }

    std::cerr << "********   observer_channel_message QUIT   ********\n";
}

// init_notify_handler 函数将传入的 fn 赋值给 notify_message_handler。
// 这样，当我想在 Redis 收到消息时执行某个操作时，你可以通过调用 init_notify_handler 来设置一个自定义的回调函数。
void Redis::init_notify_handler(std::function<void(int, std::string)> fn)
{
    this->notify_message_handler = fn;
}
// Redis::init_notify_handler 函数为 Redis 类提供了一个灵活的方式来处理接收到的消息，允许我在不修改类本身的情况下指定自定义的行为。
