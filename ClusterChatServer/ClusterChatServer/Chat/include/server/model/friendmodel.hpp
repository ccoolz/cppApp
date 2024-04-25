#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include "user.hpp"
#include <vector>

class FriendModel
{
public:
    // 添加好友关系
    bool insert(int user_id, int friend_id);

    // 通过 friend表和 user表的联合查询，返回用户的好友列表
    std::vector<User> query(int user_id);

};




#endif