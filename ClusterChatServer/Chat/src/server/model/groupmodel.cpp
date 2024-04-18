#include "groupmodel.hpp"
#include "db.h"

bool GroupModel::createGroup(Group &group)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into all_group(groupname, groupdesc) values('%s', '%s')",
        group.getName().c_str(), group.getDesc().c_str());    // id自动生成

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            group.setId( mysql_insert_id( mysql.getConnection()));  // mysql_insert_id获取这次连接数据库生成的主键 id
            return true;
        }
    }
    return false;
}

void GroupModel::addGroup(int userid, int groupid, std::string role)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into group_user values(%d, %d, '%s')", groupid, userid, role.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户所在的所有群组及其数据
std::vector<Group> GroupModel::queryGroups(int userid)
{
    // 1.根据 userid在 groupuser表中查询出该用户所属的群组有哪些，装入 group_vec中
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from all_group a inner join \
    group_user b on a.id = b.groupid where b.userid = %d", userid);

    std::vector<Group> group_vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);

        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                group_vec.push_back(group);
            }
            mysql_free_result(res);
        }
    }
    
    // 2.对于 group_vec中的每个 group，查出群组中所有用户的 id，也装入每个 group中的成员 users(vector<GroupUser>)中，用于 user在群聊中的通信
    for (Group &group : group_vec)
    {
        // 对于单个 group，通过多表查询获取它所有成员的以下四个信息，赋给 GroupUser对象
        sprintf(sql, "select a.id,a.name,a.state,b.grouprole from user a \
        inner join group_user b on b.userid = a.id where b.groupid = %d", group.getId());

        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                GroupUser user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[2]);
                group.getUsers().push_back(user);       // 在 Group类中我们维护了一个 users数组，用来装聊天群内所有用户的 id，通过 getUsers获取它
            }
            mysql_free_result(res);
        }
    }

    return group_vec;
    /*        group_vec     user_vec
    login user -> | Group1 | -> | user1 |
                  | Group2 |    | user2 |
                  | Group3 |    | user3 |
                  | Group4 |
    */
}

// 查询除 userid自己外的群成员的 id，装在 int数组里。主要用于群聊业务用户给其他成员群发消息
std::vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
    char sql[1024];
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d", groupid, userid);

    std::vector<int> id_vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                id_vec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return id_vec;
}
