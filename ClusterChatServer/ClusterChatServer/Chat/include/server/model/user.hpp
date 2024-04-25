/*
ORM（ Object-Relational Mapping，对象关系映射）框架是一种软件技术，用于将关系数据库中的数据映射到对象模型中的类和属性。
这种映射使得开发者能够使用面向对象的方式来操作数据库，从而提高了开发效率，减少了手写 SQL语句的工作量。
ORM框架的基本原理是将数据库中的表结构映射成对象模型中的类和属性。每个表对应一个类，每个字段对应一个属性。
通过这种方式，ORM框架实现了数据库与对象模型之间的转换。
*/
// 不过我们这里只是仿照 ORM框架思想，并不完全按框架思维设计
/*
User 表
    字段名称    字段类型                     字段说明         约束
    id          INT                         用户id          PRIMARY KEY、AUTO_INCREMENT
    name        VARCHAR(50)                 用户名          NOT NULL, UNIQUE
    password    VARCHAR(50)                 用户密码        NOT NULL
    state       ENUM('online', 'offline')   当前登录状态     DEFAULT 'offline'
*/
#ifndef USER_H
#define USER_H

#include <string>

// 匹配 user表的 ORM类
class User
{
public:
    User( int id_ = -1, std::string name_ = "", std::string pwd_ = "", std::string state_ = "offline")
        : id(id_)
        , name(name_)
        , password(pwd_)
        , state(state_)
    {
    }

    void setId(int id_) { this->id = id_; }
    void setName(std::string name_) { this->name = name_; }
    void setPassword(std::string pwd_) { this->password = pwd_; }
    void setState(std::string state_) { this->state = state_; }

    int getId() { return id; }
    std::string getName() { return name; }
    std::string getPassword() { return password; }
    std::string getState() { return state; }

private:
    int id;
    std::string name;
    std::string password;
    std::string state;
};



#endif