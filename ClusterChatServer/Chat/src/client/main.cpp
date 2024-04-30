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
// 记录当前用户的登录状态
bool is_online = false;

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
void logout(int clientfd, std::string cmd);

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

// 用户登录
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
            // 测试用 -- std::cout << "登录响应消息: " << buf << "\n";
            json recv_js = json::parse(buf);
            if (0 != recv_js["errno"].get<int>())
            {
                std::string errmsg = recv_js["errmsg"].get<std::string>(); // 解析后的 json会自带外层的""要用 get获取对应数值
                std::cerr << errmsg << "\n";
            }
            else
            { // 登录成功 -- 为了减小服务器的数据存储压力，用户登录成功后服务器会把用户相关信息发送给客户端，存储在客户端的本地
                is_online = true;
                // 记录当前用户的 id和姓名
                g_cur_user.setId(recv_js["id"].get<int>());
                g_cur_user.setName(recv_js["name"]);
                // 记录当前用户的好友列表，     重新登录后再 push_back，之前的记录列表信息还存在，需要先重置一下
                g_cur_user_friends.clear();
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
                // 记录当前用户的群组列表       新登录后再 push_back，之前的记录列表信息还存在，需要先重置一下
                g_cur_user_groups.clear();
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
                        std::vector<GroupUser> &group_users = group.getUsers(); // 返回引用，相当于给 group对象的成员 users取别名
                        // bug: group.getUsers()虽然返回引用，但接它的变量也需要是引用。否则就不是在操作目标 group的 users成员
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
                        if (msg_js["msgid"] == ONE_CHAT_MSG)
                        {
                            std::cout
                                << msg_js["time"].get<std::string>() << " [" << msg_js["id"].get<int>() << "-" << msg_js["name"].get<std::string>() << "]: "
                                << msg_js["msg"].get<std::string>() << "\n";
                        }
                        else if (msg_js["msgid"] == GROUP_CHAT_MSG)
                        {
                            // time + [群消息-groupid] + [id-name]: + xxx
                            std::cout 
                                << msg_js["time"].get<std::string>() << " [群消息-" << msg_js["groupid"].get<int>() << "]"
                                << " [" << msg_js["id"].get<int>() << "-" << msg_js["name"].get<std::string>() << "]: "
                                << msg_js["msg"].get<std::string>() << "\n";
                        }
                        else ;
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

// 用户注册
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
                std::cout << "\t" << user.getId() << " " << user.getName() << " " << user.getState() << " " << user.getRole() << "\n";
            }
        }
    }
    std::cout << "======================================================\n";
}

// 接收线程的线程函数
void readTaskHandler(int clientfd)
{
    while (is_online)       // #bug
    {
        // 接收到服务器转发的数据
        char buf[1024] = {0};
        int recv_len = recv(clientfd, buf, sizeof(buf), 0);
        if (recv_len <= 0)                                      // 对方主动断开连接或接收数据发生了错误
        {
            close(clientfd);
            exit(-1);
        }

        // 解析消息类型
        json recv_js = json::parse(buf);
        int msg_type = recv_js["msgid"].get<int>();

        // 根据消息类型显示 一对一聊天数据或群聊聊天 数据，正常来说不会收到其他消息类型的数据，若有，不做处理
        if (msg_type == ONE_CHAT_MSG)
        {
            std::cout << recv_js["time"].get<std::string>() << " [" << recv_js["id"].get<int>() << "-" << recv_js["name"].get<std::string>() << "]: "
                    << recv_js["msg"].get<std::string>() << "\n";
            continue;
        }
        else if (msg_type == GROUP_CHAT_MSG)
        {
            std::cout << recv_js["time"].get<std::string>() << " [群消息-" << recv_js["groupid"] << "]"
                    << " [" << recv_js["id"].get<int>() << "-" << recv_js["name"].get<std::string>() << "]: "
                    << recv_js["msg"].get<std::string>() << "\n";
            continue;
        }
        else if (msg_type == LOGIN_MSG_ACK)
        {
            // 测试用 -- std::cout << "接收子线程接收了一条登录响应消息\n";
            // 再发送当前用户的登录消息给服务器，然后 break出这个循环，结束线程函数，不会继续读走下一次登录响应信息，等待下一次登录成功唤醒此线程
            // 考虑到服务器第一次登录请求后改变了用户的 online信息会导致第二次请求失败，所以采用了下面的别的方法，如果别的方法不行，可以新建一种 “再次登录消息” 消息类型，并在两边都配套对应的处理函数
            // 如果还有问题，可以对这里和再次开启子线程处都进行上同一个锁，这样直到这个线程结束才可以开下一个子线程，考虑到客户端和服务器通信需要的时间，先不加应该没问题
            
            // 目前的处理方式是 服务器连续发两次登录消息
            // 若第一次被子线程读走，那么子线程也不会继续读第二次了，break出去，子线程结束，线程登录后开启另一个子线程。
                // 主线程读走第二次成功登录
            // 若第一次被主线程读走，主线程接下来的函数逻辑中没有读消息这一条，子线程一定会读到第二条，
                // break出去，这个子线程结束，主线程登录后开启另一个子线程

            // 更正，不应该 break
            // 主线程先接收到登录确认，先登录，然后开启子线程，子线程接收到第二条登录确认，若 break出去，无法进行接下来的正常接收聊天消息
            // 若什么也不做，分类讨论
                // （1）主线程接收到第一条，登录成功，登录后开启子线程，子线程接收到第二条，此时登录状态变量为 true，进入下一轮循环，子线程可以正常接收消息
                // （2）子线程接收到第一条，等待一小小段可以接受的时间（该时间应大于服务器发送两条消息的间隔时间），此时主线程接收到第二条登录确认，登录成功，登录状态变量立即被置 false，
                    // 登录状态变量立即被置 false，子线程无法继续接受消息从而执行结束，主线程会另开一条线程接受消息
            std::this_thread::sleep_for( std::chrono::milliseconds(150));
        }
        else
        {
            std::cerr << "客户端接收到了意外的消息\n";
            break;
        }
    }
}

// 获取当前系统时间
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
    {"logout",      "退出登录，格式 logout"}
};
// 在这个表中注册客户端支持的命令处理函数，方便在用户输入命令后自动调用对应处理
std::unordered_map<std::string, std::function<void(int, std::string)>> command_handler_map = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addFriend},
    {"creategroup", createGroup},
    {"addgroup", addGroup},
    {"groupchat", groupChat},
    {"logout", logout}
};
// 客户端主页面程序，登录后显示，提示用户可用功能及其命令
void mainMenu(int clientfd)
{
    help();
    
    while (is_online)
    {
        char buf[1024] = {0};
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
            std::cerr << "can not find command! please retry!\n";
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
        if (cmd_msg.first.size() > 6)
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
        std::cerr << "send chat message error!\n";
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

// "creategroup" command handler      creategroup:group_name:group_description
void createGroup(int clientfd, std::string cmd)
{
    // 解析命令
    int colon_idx = cmd.find(":");
    if (colon_idx == -1)
    {
        std::cerr << "invalid command! please try again!\n";
        return;
    }
    std::string group_name = cmd.substr(0, colon_idx);
    std::string group_desc = cmd.substr(colon_idx + 1, cmd.size() - 1);

    // 打包消息给服务器
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_cur_user.getId();
    js["groupname"] = group_name;
    js["groupdesc"] = group_desc;
    std::string buf = js.dump();

    int send_len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (send_len == -1)
    {
        std::cerr << "send creategroup message error!\n";
    }
}

// "addgroup" command handler        addgroup:group_id
void addGroup(int clientfd, std::string cmd)
{
    // 解析命令
    int group_id = atoi(cmd.c_str());

    // 打包消息发给服务器
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_cur_user.getId();
    js["groupid"] = group_id;
    std::string buf = js.dump();

    int send_len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (send_len == -1)
    {
        std::cerr << "send addgroup message error!\n";
    }
}

// "groupchat" command handler        groupchat:group_id:message
void groupChat(int clientfd, std::string cmd)
{
    // 解析命令
    int colon_idx = cmd.find(":");
    if (colon_idx == -1)
    {
        std::cerr << "invalid input! please retry!\n";
        return;
    }
    int group_id = atoi(cmd.substr(0, colon_idx).c_str());
    std::string msg = cmd.substr(colon_idx + 1, cmd.size() - colon_idx);

    // 打包消息发给服务器
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_cur_user.getId();
    js["time"] = currentTime();
    js["name"] = g_cur_user.getName();
    js["groupid"] = group_id;
    js["msg"] = msg;
    std::cout << "[test js] :" << js << "\n";
    std::string buf = js.dump();

    int send_len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (send_len == -1)
    {
        std::cerr << "send groupchat message error!\n";
    }
}

// "logout" command handler         logout
void logout(int clientfd, std::string cmd)
{
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = g_cur_user.getId();
    std::string buf = js.dump();

    int send_len = send(clientfd, buf.c_str(), strlen(buf.c_str())+ 1, 0);
    if (send_len == -1)
    {
        std::cerr << "send groupchat message error!\n";
        return;
    }
    // 更新用户的登录状态，以退出用户主页
    is_online = false;
}
// ----------------------------- 函数实现区 -----------------------------


/*  ___________________________________________ bug 区 ___________________________________________
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
bug_4:
    表现__
        creategroup:中文:中文失败
    原因分析__
        show create table all_group结果如下
            | all_group | CREATE TABLE `all_group` (
            `id` int(11) NOT NULL AUTO_INCREMENT COMMENT '组id',
            `groupname` varchar(50) CHARACTER SET latin1 NOT NULL COMMENT '组名称',
            `groupdesc` varchar(200) CHARACTER SET latin1 DEFAULT '' COMMENT '组功能描述',
            PRIMARY KEY (`id`),
            UNIQUE KEY `groupname` (`groupname`)
            ) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=utf8 
        发现表的默认字符集是 utf8 但`groupname``groupdesc`的字符集都是 latin1
    解决方法__
        mysql> ALTER TABLE all_group MODIFY groupname varchar(50) CHARACTER SET utf8 NOT NULL;
        mysql> ALTER TABLE all_group MODIFY groupdesc varchar(200) CHARACTER SET utf8 DEFAULT '';
bug_5:
    表现__
        比如 json解析错误等原因导致服务器程序异常结束时，已登录的用户的状态仍是 online，
        导致客户端用户的下次登录无法成功
bug_6:
    表现__
        用户登录成功后打印了用户所在群组，却没按预想打印群组中的成员信息
    原因分析__
        本文件的 login函数登录成功后获取组和组成员中有一条语句
        std::vector<GroupUser> group_users = group.getUsers(); // 返回引用，相当于给 group对象的成员 users取别名
        group.getUsers()虽然返回引用，但接它的变量也需要是引用。否则就不是在操作目标 group的 users成员
    解决方法__    
        用引用变量来接 std::vector<GroupUser> &group_users = group.getUsers();
bug_7:
    表现__
        用户登出后再登录，有时输入密码后却无响应，没有按代码逻辑进行下一步
    原因分析__
        从再次登录后的代码逻辑进行分析：用户输入密码 -> 客户端向服务器发送登录请求消息 -> 服务器回复消息(成功或失败都有响应)
        那么就是在上述过程中发生了问题，核心问题在于，服务器接收到了消息，为什么不回复消息呢？
        经过分析，我们发现原因来自子线程，在上次登录成功后，子线程开始读取服务器发送的数据，
        而登录函数所在的主线程也在等待服务器发送的返回数据，这时，若数据被子线程读走了，主线程就接收不到该数据，阻塞在了 recv()处
        这个 bug是多线程问题，无法简单通过调试排出来，需要对程序了解熟悉过程，思考出来
    解决方法__
        （1）
            设置一个变量 bool is_online，设置子线程循环接收信息的循环条件为 is_online
            这样退出登录后就无法继续这个循环，子线程函数执行结束，退出子线程
            此时出现频率已大大减少，但还会有上述错误情况，应该是子线程此时阻塞在 recv，在循环体内，还是会可能读走数据
            那么就要想办法解决这个登录数据被读走的情况
                （目前想到的解决方案是 子线程的 登录中还应该有一个发送回应消息给服务器的部分
                当服务器发送的登录确认消息后开始 recv（这里应该用 muduo的接收 api）子线程 login()发送的登录确认消息
                若一段时间后还没有收到确认消息，说明发送的登录响应消息被客户端子线程读走，此时子线程函数进入判断，发现用户已下线，子线程结束
                那么再发送一次登录响应消息就一定会被客户端主线程的登录程序接收。这个思路类似三次握手，客户端发登录消息 - 服务端发登录确认消息 - 客户端发确认登录成功消息 的过程
                还想到一种方法，子线程对收到的消息进行判断时，对收到消息为登录响应消息的情况进行处理（未完待续），先用这种方法确认是否是这个原因
                括号内的想法未采用）
        （2）
            目前的处理方式是 ，在（1）的基础上，服务器连续发两次登录消息
            若第一次被子线程读走，那么子线程也不会继续读第二次了，break出去，子线程结束，线程登录后开启另一个子线程。主线程读走第二次成功登录
            若第一次被主线程读走，主线程接下来的函数逻辑中没有读消息这一条，子线程一定会读到第二条，break出去，这个子线程结束，主线程登录后开启另一个子线程
            // 更正，不应该 break
            // 主线程先接收到登录确认，先登录，然后开启子线程，子线程接收到第二条登录确认，若 break出去，无法进行接下来的正常接收聊天消息
            // 若什么也不做，分类讨论
                // （1）主线程接收到第一条，登录成功，登录后开启子线程，子线程接收到第二条，此时登录状态变量为 true，进入下一轮循环，子线程可以正常接收消息
                // （2）子线程接收到第一条，等待一小小段可以接受的时间，此时主线程接收到第二条登录确认，登录成功，登录状态变量立即被置 false，
                    // 子线程无法继续接受消息从而执行结束，主线程会另开一条线程接受消息
    ___________________________________________ bug 区 ___________________________________________ */


/*  ______________________________________ 待完善功能区 ______________________________________
(1) 接收数据部分除了聊天类型信息，还可以对别的信息进行处理，这需要我们现在服务器代码中对某些处理完的信息进行返回
    比如可以在服务器程序创建群聊后发一个创建群聊响应消息给客户端，客户端根据错误码打印创建成功或创建失败给用户看
    比如可以在其他好友用户登录时实时更新好友状态表给用户看，以告知用户哪些好友在线
    ______________________________________ 待完善功能区 ______________________________________*/