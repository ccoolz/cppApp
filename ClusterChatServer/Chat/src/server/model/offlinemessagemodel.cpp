#include "offlinemessagemodel.hpp"
#include "db.h"

// 存储用户的离线消息
void OfflineMsgModel::insert(int userid, std::string msg)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into offline_message values(%d,'%s')", userid, msg.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 移除已转发给用户的离线消息
void OfflineMsgModel::remove(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from offline_message where userid = %d", userid);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户的离线消息
std::vector<std::string> OfflineMsgModel::query(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select message from offline_message where userid = %d", userid);

    MySQL mysql;
    std::vector<std::string> msgs_vec;

    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);

        if (res != nullptr)                 // 没有 message返回 nullptr
        {        
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                msgs_vec.push_back(row[0]);
            }
            
            mysql_free_result(res);
        }
        /* 假如查询到的结果是
        +---------+
        | message |
        -----------
        | hello   |
        | ok      |
        +---------+
            mysql_fetch_row 会从上到下一行行的读取，返回给 MYSQL_ROW 对象，读到结束返回 nullptr
            因为 select message 而不是 select * 所以读到的 row里只有 row[0]也就是 (string)消息    */
    }
    return msgs_vec;
}
