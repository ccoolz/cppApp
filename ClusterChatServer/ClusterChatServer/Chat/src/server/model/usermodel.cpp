#include "usermodel.hpp"
#include "db.h"

// 用户表的插入用户操作
bool UserModel::insert(User &user)
{
    // 1. 组装 sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into user(name, password, state) values('%s', '%s', '%s')",
    user.getName().c_str(), user.getPassword().c_str(), user.getState().c_str());       // id 是插入数据后自动生成且自动递增的

    // 2. 连接数据库
    MySQL mysql;
    if (mysql.connect())
    {                       // 3. 更新这条 sql语句
        if (mysql.update(sql))
        {                   // 4. 插入成功后用户会生成主键 id，获取 id并赋给用户表的 ORM类对象
            user.setId( mysql_insert_id( mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// 根据用户 id号查询用户信息并返回
User UserModel::query(int id)
{
    char sql[1024] = {0};
    sprintf(sql, "select * from user where id = %d", id);

    MySQL mysql;
    if (mysql.connect())
    {                       
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)                         // 没查找到返回空指针
        {
            MYSQL_ROW row = mysql_fetch_row(res);   // 获取行（也就是一个用户的数据）
            if (row != nullptr)                     // 未获取到行
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setPassword(row[2]);
                user.setState(row[3]);
                mysql_free_result(res);             // MYSQL_RES指针资源需要释放
                return user;
            }
        }
    }

    return User();  // User默认构造 id = -1，以此判断查找失败
}

// 更新用户的状态（state）信息
bool UserModel::updateState(User user)
{
    char sql[1024] = {0};
    sprintf(sql, "update user set state = '%s' where id = %d", user.getState().c_str(), user.getId());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql) == true)
        {
            return true;
        }
    }
    return false;
}

// 重置所有 online用户的状态信息为 offline
void UserModel::resetState()
{
    char sql[1024] = "update user set state = 'offline' where state = 'online'";

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
