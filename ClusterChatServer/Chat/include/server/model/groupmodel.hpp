#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"

// 封装群组业务相关数据库操作的接口
class GroupModel
{
public:
    // 创建群聊
    bool createGroup(Group &group);
    
    // 加入群聊
    void addGroup(int userid, int groupid, std::string role);

    // 查询用户所在的所有群组及其数据
    std::vector<Group> queryGroups(int userid);

    // 查询群聊所有用户的 id号，主要用于群聊业务给群聊其他成员群发消息
    std::vector<int> queryGroupUsers(int userid, int groupid);
};

#endif