#ifndef OFFLINEMESSAGEMODEL_H
#define OFFLINEMESSAGEMODEL_H

#include <string>
#include <vector>

class OfflineMsgModel
{
public:
    // 存储用户的离线消息
    void insert(int userid, std::string msg);

    // 移除已转发给用户的离线消息
    void remove(int userid);
    
    // 查询用户的离线消息
    std::vector<std::string> query(int userid);
};

#endif