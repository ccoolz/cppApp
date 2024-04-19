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
// 显示当前登录成功用户的基本信息
void showCurUserData();
// 接收线程的线程函数       cin等待输入阻塞在那里，当然不能没发送消息的时候也不能接收信息，所以对接收和发送要分别用线程并行执行，主线程发送，子线程接收
void readTaskHandler(int clientfd);
// 获取当前系统时间（聊天信息需添加时间信息）
std::string currentTime();
// 用户主页面
void mainMenu();


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
// 显示当前登录成功用户的基本信息
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
        case 1: // login
        {   
            // 引导用户输入 id和 密码
            int id = 0;
            char pwd[50] = {0};
            std::cout << "your id:";
            std::cin >> id;
            std::cin.get();                 // 读掉缓冲区残留的回车
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
                                0);         // +1可以把'\0'也发出去
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
                    if (0 != recv_js["errno"].get<int>() )
                    {
                        std::string errmsg = recv_js["errmsg"].get<std::string>();  // 解析后的 json会自带外层的""要用 get获取对应数值
                        std::cerr << errmsg << "\n";
                    }
                    else
                    {   // 登录成功 -- 为了减小服务器的数据存储压力，用户登录成功后服务器会把用户相关信息发送给客户端，存储在客户端的本地
                        // 记录当前用户的 id和姓名
                        g_cur_user.setId( recv_js["id"].get<int>() );
                        g_cur_user.setName( recv_js["name"] );
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
                                std::vector<GroupUser> group_users = group.getUsers();      // 返回引用，相当于给 group对象的成员 users取别名
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
                        if (recv_js.contains("offlinemsg") )
                        {
                            std::vector<std::string> msg_strs = recv_js["offlinemsg"];
                            for (std::string &msg_str : msg_strs)
                            {
                                json msg_js = json::parse(msg_str);
                                // time + [id] + name + " said: " + xxx
                                std::cout
                                    << msg_js["time"] << " [" << msg_js["id"] << "]" << msg_js["name"]
                                    << " said: " << msg_js["msg"] << "\n";
                            }
                        }

                        // 登录成功，启动接收线程负责接收数据
                        std::thread readTask(readTaskHandler, clientfd);
                        readTask.detach();
                        // 进入用户主页
                        mainMenu();
                    }
                }
            }
        }
        break;  // 跳出 switch，进入 for循环
        case 2: // register
        {
            // 引导用户输入 id和 密码
            char name[50] = {0};
            char pwd[50] = {0};
            std::cout << "username:";
            std::cin.getline(name, 50);     // cin.get()遇到空格换行，无法输入"zhang san"这样的名字，getline()遇到回车换行，且会丢弃换行符，行尾自动添加'\0'
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
                                strlen(regist_req.c_str()) + 1,         // 用 sizeof(char *)会返回指针大小而非字符串大小
                                0);
            if (send_len == -1)               // =0是对方主动关闭了连接
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
                    if (0 != recv_js["errno"].get<int>() )              // 约定错误码=0为注册成功
                    {
                        // 注册失败即数据库插入语句失败，因为 name是 UNIQUE的，所以是姓名重复，提醒用户错误
                        std::cerr << "username:" << name << " is already exist, register failed!\n";
                    }
                    else
                    {   // 注册成功
                        std::cout << "register successfully! userid is " << recv_js["id"]
                                  << ", please keep it in mind!\n";
                    }
                }
            }
        }
        break;  // 跳出 switch，进入 for循环
        case 3: // quit
            close(clientfd);
            exit(0);
        default:
            std::cerr << "invalid input!try again!\n";
            break;  // 跳出 switch，进入 for循环
        }
    }

}

// 显示当前登录用户的数据
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

void readTaskHandler(int clientfd)
{
}

std::string currentTime()
{
    return std::string();
}

void mainMenu()
{

}
// ----------------------------- 函数实现区 -----------------------------
