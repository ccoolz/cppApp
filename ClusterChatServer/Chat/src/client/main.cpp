// C++标准库及三方库
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include "json.hpp"
using json = nlohmann::json;
// Linux C/C++库
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// 自建头文件
#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前登录用户的信息
User g_cur_user;
// 记录当前登录用户的好友列表
std::vector<User> g_cur_user_friends;
// 记录当前登录用户的群组列表
std::vector<Group> g_cur_user_groups;

// 注册登录首页
void homepage(int clientfd);
// 用户登录
void login(int clientfd);
// 用户注册
void regist(int clientfd);
// 显示当前登录成功用户的基本信息
void showCurUserData();
// 接收线程的线程函数       cin等待输入阻塞在那里，当然不能没发送消息的时候也不能接收信息，所以对接收和发送要分别用线程并行执行，主线程发送，子线程接收
void readTaskHandler(int clientfd);
// 获取当前系统时间（聊天信息需添加时间信息）
std::string currentTime();
// 用户主页面
void mainMenu(int clientfd);
// command handler
void help(int clientfd = 0, std::string cmd = "");
void chat(int clientfd, std::string cmd);
void addFriend(int clientfd, std::string cmd);
void createGroup(int clientfd, std::string cmd);
void addGroup(int clientfd, std::string cmd);
void groupChat(int clientfd, std::string cmd);
void loginOut(int clientfd, std::string cmd);

int main(int argc, char **argv)
{
    if (argc < 3)
    {               // 客户端启动时需要输入服务器的 ip地址和端口
        std::cerr << "Command invalid! example: ./ChatClient 127.0.0.1 5000\n";
        exit(-1);
    }

    // 创建客户端 socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        std::cerr << "socket create error\n";
        exit(-1);
    }

    // 解析通过命令行参数传递的 ip和 port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);
    // 填写服务器的网络地址
    sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);               // bug: 没有把本地字节序转网络字节序
    saddr.sin_addr.s_addr = inet_addr(ip);      // inet_addr() 函数的作用是将点分十进制的IPv4地址转换成网络字节序列的长整型。

    // client向 server请求连接
    if (-1 == connect(clientfd, (sockaddr*)&saddr, sizeof(sockaddr_in)))
    {
        std::cerr << "connect server error\n";
        close(clientfd);
        exit(-1);
    }

    // main()所在线程用于接收用户输入，发送数据
    homepage(clientfd);

    return 0;
}


// ----------------------------- 函数实现区 -----------------------------,
// 注册登录首页
void homepage(int clientfd)
{
    for (;;)
    {
        // 显示页面首页菜单 登录、注册、退出
        std::cout 
            << "======================\n"
            << "1. login\n"
            << "2. register\n"
            << "3. quit\n"
            << "======================\n"
            << "choice:";
        int choice = 0;
        std::cin >> choice;
        std::cin.get();                     // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1:     // login
            login(clientfd);
            break;              // 跳出 switch，进入 for循环
        case 2:     // register
            regist(clientfd);
            break;  
        case 3:     // quit
            close(clientfd);
            exit(0);
        default:
            std::cerr << "invalid input!try again!\n";
            break;
        }
    }

}

void login(int clientfd)
{
    // 引导用户输入 id和 密码
    int id = 0;
    char pwd[50] = {0};
    std::cout << "your id:";
    std::cin >> id;
    std::cin.get(); // 读掉缓冲区残留的回车
    std::cout << "password:";
    std::cin.getline(pwd, 50);

    // json封装登录请求消息，并序列化
    json js;
    js["msgid"] = LOGIN_MSG;
    js["id"] = id;
    js["password"] = pwd;
    std::string login_req = js.dump();

    // 发送登录请求给服务器
    int send_len = send(clientfd,
                        login_req.c_str(),
                        strlen(login_req.c_str()) + 1,
                        0); // +1可以把'\0'也发出去
    if (send_len == -1)
    {
        std::cerr << "send login message error!\n";
    }
    else
    {
        // 发送成功，接收回应消息，若回应消息长度正常则解析获取相关数据
        char buf[1024] = {0};
        int recv_len = recv(clientfd, buf, sizeof(buf), 0);
        if (recv_len == -1)
        {
            std::cerr << "recv login response message error!\n";
        }
        else
        {
            // 登录失败有两种情况 -- 用户已登录 | 账号或密码错误，通过 errmsg返回
            json recv_js = json::parse(buf);
            if (0 != recv_js["errno"].get<int>())
            {
                std::string errmsg = recv_js["errmsg"].get<std::string>(); // 解析后的 json会自带外层的""要用 get获取对应数值
                std::cerr << errmsg << "\n";
            }
            else
            { // 登录成功 -- 为了减小服务器的数据存储压力，用户登录成功后服务器会把用户相关信息发送给客户端，存储在客户端的本地
                // 记录当前用户的 id和姓名
                g_cur_user.setId(recv_js["id"].get<int>());
                g_cur_user.setName(recv_js["name"]);
                // 记录当前用户的好友列表
                if (recv_js.contains("friends"))
                {
                    std::vector<std::string> friends = recv_js["friends"];
                    for (std::string &fri_str : friends)
                    {
                        json fri_js = json::parse(fri_str);
                        User fri;
                        fri.setId(fri_js["id"]);
                        fri.setName(fri_js["name"]);
                        fri.setState(fri_js["state"]);
                        g_cur_user_friends.push_back(fri);
                    }
                }
                // 记录当前用户的群组列表
                if (recv_js.contains("groups"))
                {
                    std::vector<std::string> groups_str_vec = recv_js["groups"];
                    for (std::string &group_str : groups_str_vec)
                    {
                        json group_js = json::parse(group_str);
                        Group group;
                        group.setId(group_js["id"].get<int>());
                        group.setName(group_js["groupname"]);
                        group.setDesc(group_js["groupdesc"]);
                        std::vector<GroupUser> group_users = group.getUsers(); // 返回引用，相当于给 group对象的成员 users取别名
                        std::vector<std::string> users_str_vec = group_js["users"];
                        for (std::string &user_str : users_str_vec)
                        {
                            json user_js = json::parse(user_str);
                            GroupUser user;
                            user.setId(user_js["id"]);
                            user.setName(user_js["name"]);
                            user.setState(user_js["state"]);
                            user.setRole(user_js["role"]);
                            group_users.push_back(user);
                        }
                        g_cur_user_groups.push_back(group);
                    }
                }

                // 显示登录用户的基本信息
                showCurUserData();
                // 显示登录用户的离线消息
                if (recv_js.contains("offlinemsg"))
                {
                    std::vector<std::string> msg_strs = recv_js["offlinemsg"];
                    for (std::string &msg_str : msg_strs)
                    {
                        json msg_js = json::parse(msg_str);
                        // time + [id name]: + xxx
                        std::cout
                            << msg_js["time"].get<std::string>() << " [" << msg_js["id"].get<int>() << "-" << msg_js["name"].get<std::string>() << "]: "
                            << msg_js["msg"].get<std::string>() << "\n";
                    }
                }

                // 登录成功，启动接收线程负责接收数据（如其他服务器转发过来的聊天消息）
                std::thread readTask(readTaskHandler, clientfd); // 底层调用 pthread_create
                readTask.detach();                               // pthread_detach
                // 进入用户主页
                mainMenu(clientfd);
            }
        }
    }
}

void regist(int clientfd)
{
    // 引导用户输入 id和 密码
    char name[50] = {0};
    char pwd[50] = {0};
    std::cout << "username:";
    std::cin.getline(name, 50); // cin.get()遇到空格换行，无法输入"zhang san"这样的名字，getline()遇到回车换行，且会丢弃换行符，行尾自动添加'\0'
    std::cout << "password:";
    std::cin.getline(pwd, 50);

    // 封装注册请求消息
    json js;
    js["msgid"] = REGIST_MSG;
    js["name"] = name;
    js["password"] = pwd;
    std::string regist_req = js.dump();

    // 发送注册请求给服务器
    int send_len = send(clientfd,
                        regist_req.c_str(),
                        strlen(regist_req.c_str()) + 1, // 用 sizeof(char *)会返回指针大小而非字符串大小
                        0);
    if (send_len == -1) // =0是对方主动关闭了连接
    {
        std::cerr << "send register message error!\n";
    }
    else
    {
        // 发送成功，接收注册请求的回应消息
        char buf[1024] = {0};
        int recv_len = recv(clientfd, buf, sizeof(buf), 0);
        if (recv_len == -1)
        {
            std::cerr << "recv register response message error!\n";
        }
        else
        {
            // 接收回应成功，解析注册结果json
            json recv_js = json::parse(buf);
            if (0 != recv_js["errno"].get<int>()) // 约定错误码=0为注册成功
            {
                // 注册失败即数据库插入语句失败，因为 name是 UNIQUE的，所以是姓名重复，提醒用户错误
                std::cerr << "username:" << name << " is already exist, register failed!\n";
            }
            else
            { // 注册成功
                std::cout << "register successfully! userid is " << recv_js["id"]
                          << ", please keep it in mind!\n";
            }
        }
    }
}

// 显示当前登录成功用户的基本信息
void showCurUserData()
{
    std::cout 
        << "======================login user======================\n"
        << "current login user => id:" << g_cur_user.getId() << " name:" << g_cur_user.getName() << "\n"
        << "----------------------friend list----------------------\n";
    if (g_cur_user_friends.empty() == false)
    {
        for (User &user : g_cur_user_friends)
        {
            std::cout << user.getId() << " " << user.getName() << " " << user.getState() << "\n";
        }
    }
    std::cout 
        << "----------------------group list----------------------\n";
    if (g_cur_user_groups.empty() == false)
    {
        for (Group &group : g_cur_user_groups)
        {
            std::cout << group.getId() << " " << group.getName() << " " << group.getDesc() << "\n";
            for (GroupUser &user : group.getUsers())
            {
                std::cout << user.getId() << " " << user.getName() << " " << user.getState() << " " << user.getRole() << "\n";
            }
        }
    }
    std::cout << "======================================================\n";
}

// 接收线程 的线程函数
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        // 接收到服务器转发的数据
        char buf[1024] = {0};
        int recv_len = recv(clientfd, buf, sizeof(buf), 0);
        if (recv_len <= 0)                                      // 对方主动断开连接或接收数据发生了错误
        {
            close(clientfd);
            exit(-1);
        }
        json recv_js = json::parse(buf);

        // 处理一对一聊天数据
        if (recv_js["msgid"].get<int>() == ONE_CHAT_MSG)
        {
            std::cout << recv_js["time"].get<std::string>() << " [" << recv_js["id"].get<int>() << "-" << recv_js["name"].get<std::string>() << "]: "
                    << recv_js["msg"].get<std::string>() << "\n";
            continue;
        }
    }
}

std::string currentTime()
{
    // chrono库 system_clock::now() 方法获取系统时钟当前时间，外面套 system_clock::to_time_t() 方法转获取的时间为 time_t类型
    auto time_t_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    // time.h库 localtime() 方法传入 time_t类型变量的地址，返回 tm类型结构体的指针，tm结构体包含年月日等多个描述时间的成员
    tm *p_tm = localtime(&time_t_now);
    char now[60] = {0};
    sprintf(now, "%d-%02d-%02d %02d:%02d:%02d",
            (int)p_tm->tm_year + 1900, (int)p_tm->tm_mon + 1, (int)p_tm->tm_mday,
            (int)p_tm->tm_hour, (int)p_tm->tm_min, (int)p_tm->tm_sec);
    // 返回字符串示例: 2020-02-21 21:49:19      （年-月-日 时-分-秒）
    // %02d：按宽度为 2输出，右对齐方式。位数不够，左边补 0
    return std::string(now);
}

// 支持的客户端命令列表
std::unordered_map<std::string, std::string> command_map = {
    {"help",        "显示客户端支持的命令，格式 help"},
    {"chat",        "一对一聊天，格式 chat:friend_id:message"},
    {"addfriend",   "添加好友，格式 addfriend:friend_id"},
    {"creategroup", "创建群组，格式 creategroup:group_name:group_description"},
    {"addgroup",    "加入群组，格式 addgroup:group_id"},
    {"groupchat",   "群聊，格式 groupchat:group_id:message"},
    {"loginout",    "退出登录，格式 loginout"}
};
// 在这个表中注册客户端支持的命令处理函数，方便在用户输入命令后自动调用对应处理
std::unordered_map<std::string, std::function<void(int, std::string)>> command_handler_map = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addFriend},
    {"creategroup", createGroup},
    {"addgroup", addGroup},
    {"groupchat", groupChat},
    {"loginout", loginOut}
};
// 客户端主页面程序，登录后显示，提示用户可用功能及其命令
void mainMenu(int clientfd)
{
    help();

    char buf[1024] = {0};
    for (;;)
    {
        // 获取用户输入的完整命令
        std::cin.getline(buf, sizeof(buf));
        std::string cmd_buf(buf);
        // 解析用户的命令
        int colon_idx = cmd_buf.find(":");
        std::string cmd;
        if (colon_idx == -1)                // string的 find函数找不到返回-1
            cmd = cmd_buf;
        else
            cmd = cmd_buf.substr(0, colon_idx);
        // 若 command_handler_map中找不到该命令，提示用户错误信息
        auto iter = command_handler_map.find(cmd);
        if (iter == command_handler_map.end() )
        {
            std::cerr << "invalid command! please retry!\n";
            continue;
        }
        // 找到了命令，调用对应的处理函数，同样的，通过 map表注册 cmd-handler的映射，达到了不用添加业务代码的效果
        std::function<void (int, std::string)> cmd_handler = iter->second;
        cmd_handler(clientfd, cmd_buf.substr(colon_idx + 1, cmd_buf.size() - colon_idx));
    }
}

// "help" command handler
void help(int, std::string)
{
    std::cout << "<<<<<    COMMAND LIST    >>>>>\n";
    for (const std::pair<std::string, std::string> &cmd_msg : command_map)
    {
        if (cmd_msg.first.size() > 4)
            std::cout << cmd_msg.first << ":\t" << cmd_msg.second << "\n";
        else
            std::cout << cmd_msg.first << ":\t\t" << cmd_msg.second << "\n";
    }
    std::cout << "按格式输入您的命令:\n";
}

// "chat" command handler           chat:friend_id:message
void chat(int clientfd, std::string cmd)
{
    // 解析命令
    int colon_idx = cmd.find(":");
    if (colon_idx == -1) 
    {
        std::cerr << "invalid command! please try again!\n";
        return;
    }
    int friend_id = atoi(cmd.substr(0, colon_idx).c_str());
    std::string msg = cmd.substr(colon_idx + 1, cmd.size() - colon_idx);

    // 打包一个“一对一聊天”类型的 json消息给服务器    {"msgid":5,"id":1,"name":"zhang san","toid":2,"msg":"hello"}
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_cur_user.getId();
    js["name"] = g_cur_user.getName();
    js["toid"] = friend_id;
    js["msg"] = msg;
    js["time"] = currentTime();
    std::string buf = js.dump();        // #bug_1

    int send_len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (send_len == -1)
    {
        std::cerr << "send addfriend message error!\n";
    }
}

// "addfriend" command handler      addfriend:friend_id
void addFriend(int clientfd, std::string cmd)
{
    // 解析命令
    int friend_id = atoi(cmd.c_str());
    
    // 打包一个“添加好友”类型的 json消息给服务器    {"msgid":6,"id":1,"friendid":2}
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_cur_user.getId();
    js["friendid"] = friend_id;
    std::string buf = js.dump();

    int send_len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (send_len == -1)
    {
        std::cerr << "send addfriend message error!\n";
    }
}

void createGroup(int clientfd, std::string cmd)
{
}

void addGroup(int clientfd, std::string cmd)
{
}

void groupChat(int clientfd, std::string cmd)
{
}

void loginOut(int clientfd, std::string cmd)
{
    homepage(clientfd);
}

// ----------------------------- 函数实现区 -----------------------------


/* __________________________ bug 区 __________________________
gdb调试 :
    gdb ../bin/ChatServer       调试可执行文件
    break chatserver.cpp:41     设断点
    run                         开始运行
    c(continue)                 继续运行
    n(next)                     一行行运行
_______
bug_1: 
    表现__
        若使用 const char* 接 json.dump().c_str()数据，如
        const char *buf = js.dump().c_str()
        vscode 不会报错 | vs会报错 C26815: 指针无关联，因为它指向已销毁的临时实例。
        用这种方式接下来的是无效的 const char* 数据
    原因分析__
        .dump()返回的是临时 string 对象，这一行语句结束，string 对象自动销毁
        它底层的 char 数组也被销毁，那么 const char* 指向的地址就失效，造成了悬空指针的问题
    解决方法__
        用 std::string 对象去接 .dump()返回的对象临时，相当于构造了一个局部变量，
        再用该局部变量进行返回 c_str 等操作
        这中间的主要区别：用指针类型指向一个临时变量 or 用临时变量的数据构造一个新的变量
bug_2: 
    表现__
        当聊天用户离线时，离线消息会被存储到数据库
        当离线消息中包含中文，某些消息插入数据库的操作会失败，某些却会成功，如"你好""还可以吧"可以成功插入，
        不过在数据库中是乱码，读到程序中却又显示为中文
        mysql_error(conn)打印 Incorrect string value: '\x89","ms...' for column 'message' at row 1
    原因分析__
        预计可能是数据库/json/终端字符集中的某个或多个有问题，或不一致导致的
    解决方法
        vscode 右下角编码格式就是默认的 UTF-8 
                -- 经测试，改为 GBK结果相同，不是 vscode编码的问题
        db.cpp:39行处代码为 mysql_query(conn, "set names utf8"); 
                -- 改为 set names gbk出现上述错误 Incorrect string value
        在 linux 中进入 mysql chat表，执行ALTER TABLE offline_message DEFAULT CHARACTER SET gbk;
                -- 经测试，不管 SET gbk 还是 SET utf8，都没有问题
    综上所述
        核心原因就是需要在连接数据库后设置进行操作 mysql_query(conn, "set names utf8");  它表现为
        character_set_client    | utf8
        character_set_connection| utf8
        character_set_results   | utf8
        而在这之前使用的语句 mysql_query(conn, "set names gbk");这会导致不兼容的问题从而中文数据无法插入数据库
bug_3: 
    表现__
        发送多条消息给离线用户，用户上先后只会收到一条消息
    原因分析__
        这是离线消息表设计上的问题，一开始把表的 userid设为主键，导致一个 userid只会有一条数据被储存
    解决方法__
        重新建表，userid不设为主键，设为 not null即可
__________________________ bug 区 __________________________ */