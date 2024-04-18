#include "friendmodel.hpp"
#include "db.h"

bool FriendModel::insert(int user_id, int friend_id)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values(%d, %d)", user_id, friend_id);

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            return true;
        }
    }
    return false;
}

// 通过 friend表和 user表的联合查询，返回用户的好友列表
std::vector<User> FriendModel::query(int user_id)
{
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.name,a.state from user a inner join friend b on a.id = b.friendid where b.userid = %d", user_id);

    std::vector<User> friends;
    MySQL mysql;
    if (mysql.connect())                            // 连接数据库
    {
        MYSQL_RES *res = mysql.query(sql);          // 查询，返回一行行结果
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {                                       // 一行行读取，读取结果都是字符串形式
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                friends.push_back(user);
            }
            mysql_free_result(res);
        }
    }

    return friends;
}
