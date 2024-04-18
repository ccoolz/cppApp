#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

// 群组用户，比普通用户多一个 role角色信息，从 User类直接继承，复用 User的其他信息
class GroupUser : public User
{
public:
    void setRole(std::string role_) { role = role_; }
    std::string getRole() {return role; }

private:
    std::string role;
};


#endif