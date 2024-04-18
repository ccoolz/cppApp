#ifndef GROUP_H
#define GROUP_H

#include <string>
#include <vector>
#include "groupuser.hpp"

// all_group表的 ORM类
class Group
{
public:
    Group(int id_ = -1, std::string name_ = "", std::string desc_ = "")
    {
        id = id_;
        name = name_;
        desc = desc_;
    }

    void setId(int id_) { id = id_; }
    void setName(std::string name_) { name = name_; }
    void setDesc(std::string desc_) { desc = desc_; }
    
    int getId() { return id; }
    std::string getName() { return name; }
    std::string getDesc() { return desc; }
    std::vector<GroupUser>& getUsers() { return users;}

private:
    int id;             // 群聊 id
    std::string name;   // 群聊名称
    std::string desc;   // 群聊描述
    std::vector<GroupUser> users;   // 群聊成员 -- 通过和 group_user表的联合查询获取
};


#endif