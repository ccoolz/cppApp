// server和 client的公共代码
#ifndef PUBLIC_H
#define PUBLIC_H

enum MsgType 
{
    LOGIN_MSG = 1,          // 登录消息
    LOGIN_MSG_ACK,          // 登录响应消息
    REGIST_MSG,             // 注册消息
    REGIST_MSG_ACK,         // 注册响应消息
    ONE_CHAT_MSG,           // 一对一聊天消息
    ADD_FRIEND_MSG,         // 添加好友消息
    CREATE_GROUP_MSG,       // 创建群聊
    ADD_GROUP_MSG,          // 加入群聊
    GROUP_CHAT_MSG,         // 群聊天
};

/*
{"msgid":1,"password":"123"}
登录
{"msgid":1,"id":1,"password":"123456"}
{"msgid":1,"id":2,"password":"123"}
注册
{"msgid":3,"name":"zhang san","password":"123456"}
一对一聊天
{"msgid":5,"id":1,"name":"zhang san","toid":2,"msg":"hello","time":"2020-02-21 21:49:19"}
{"msgid":5,"id":2,"name":"li si","toid":1,"msg":"nice to meet you","time":"2020-02-21 21:49:20"}
添加好友
{"msgid":6,"id":1,"friendid":2}
{"msgid":6,"id":2,"friendid":1}
*/

/*
friend和 user表的联合查询
通过 userid查到 用户所有的 friendid，通过这些 id在 user表里查出 friend的（id, name, state）数据。注意，不能查 password
select a.id,a.name,a.state from user a inner join friend b on b.friendid = a.id where b.userid = %d
查找 friend表中 userid = ? 的所有 friendid对应 user表中 id的 id, name, state数据
inner join: 一种连接方式，从多个表中仅返回匹配的记录集合。
*/
#endif