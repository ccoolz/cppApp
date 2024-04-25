// 封装数据库操作模块
#ifndef DB_H
#define DB_H
#include <string>
#include <mysql/mysql.h>

// 数据库操作类
class MySQL
{
public:
    MySQL();                            // 初始化数据库连接
    ~MySQL();                           // 释放数据库连接资源
    bool connect();                     // 连接数据库
    bool update(std::string sql);       // 更新操作
    MYSQL_RES* query(std::string sql);  // 查询操作
    MYSQL* getConnection();             // 获取连接

private:
    MYSQL *conn;
};

#endif