#include "db.h"
#include <muduo/base/Logging.h>
#include <iostream>

// 数据库配置信息
static std::string server = "127.0.0.1";
static std::string user = "root";
static std::string password = "123456";
static std::string dbname = "chat";

// 初始化数据库连接
MySQL::MySQL()
{
    conn = mysql_init(nullptr);
}

// 释放数据库连接资源
MySQL::~MySQL()
{
    if (conn != nullptr)
        mysql_close(conn);
}

// 连接数据库
bool MySQL::connect()
{
                                // 操作数据库的操作相当于 我们的程序是客户端，MySQL是服务器
                                // MySQL连接初始化后会监听在 3306端口，本机 ip向 MySQL发起连接，连接失败返回空指针
    MYSQL *p = mysql_real_connect(  conn,               // MYSQL*
                                    server.c_str(),     // ip of our server( MySQL client)
                                    user.c_str(),       // mysql user
                                    password.c_str(),   // user pwd
                                    dbname.c_str(),     // db name
                                    3306,               // mysql default listen port
                                    nullptr,            // uinx fd
                                    0);                 // flag
    if (p != nullptr)
    {                           // C/C++默认编码是 ASCII，若不设置 gbk，从 MySQL拉下来的中文会显示乱码
        mysql_query(conn, "set names utf8");
        LOG_INFO << "connect mysql succeeded!" ;
    }
    else
    {
        LOG_INFO << "connect mysql failed!";
    }
    
    return p;                   // 若连接失败，nullptr 也相当于0，即 bool值 false
}

// 更新操作 -- 增删改，返回成功失败即可
bool MySQL::update(std::string sql)
{
    if (mysql_query(conn, sql.c_str()) != 0)            // mysql_query 执行 sql语句成功返回 0
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << " 更新失败！";
        std::cerr << "mysql error: " << mysql_error(conn);
        return false;
    }
    return true;
}

// 查询操作 -- 查询要返回查询结果
MYSQL_RES* MySQL::query(std::string sql)
{
    if (mysql_query(conn, sql.c_str()) != 0)
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败！";
        return nullptr;
    }
    return mysql_use_result(conn);
}

// 获取连接
MYSQL* MySQL::getConnection()
{
    return conn;
}
