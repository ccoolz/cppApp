// 封装数据操作模块，业务模块只需调用这边的接口，无需关注操作数据库的细节
#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

// User表的数据操作类（增删改查）
class UserModel
{
public:
    bool insert(User& user);        // 用户表的插入用户操作
    User query(int id);             // 根据用户 id号查询用户信息
    bool updateState(User user);    // 更新用户的状态（state）信息
    void resetState();              // 重置所有用户的状态信息为 offline
};


#endif