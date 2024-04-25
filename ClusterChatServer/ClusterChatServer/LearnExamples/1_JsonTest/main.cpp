// 使用json库必备的两行语句
#include "json.hpp"     // json for modern c++
using json = nlohmann::json;
// json存储信息，可用于不通用语言之间的交流和信息传递

#include <iostream>
#include <map>
#include <vector>
#include <string>

// json序列化示例1
void func1() 
{
    json js;    // 里面存储的是一个个键值对
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "nice to meet u";

    std::cout << js << std::endl;   // 库中重载了<<
}

// json序列化示例2
void func2() 
{               // 要把json数据通过网络发送出去，肯定需要将数据转成字符串(char*)形式
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "nice to meet u";

    std::string sendBuf = js.dump();                // dump有转存、打印输出的意思
    std::cout << sendBuf.c_str() << std::endl;      // 在网络上传输时传输char*
}

// json序列化示例3
void func3() 
{               // json的键值相当灵活，甚至可以用二维数组的形式 -- 键的值也可以作为键，它也有值
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"]["zhang san"] = "nice to meet u";
    js["msg"]["li si"] = "nice to meet u too";

    std::cout << js << std::endl;
}

// json序列化示例3
std::string func4()
{               // json还有个非常强大的功能 -- 可以直接序列化容器
    json js;

    // 序列化一个vector容器
    std::vector<int> vec = { 1,2,3,4,5 };
    js["int list"] = vec;

    std::map<int, std::string> map;
    map.emplace(1, "黄山");
    map.emplace(2, "华山");
    map.emplace(3, "泰山");
    js["mountains"] = map;

    std::cout << js << std::endl;

    // 序列化的定义                                     序列化
    std::string sendBuffer = js.dump(); // json数据对象   ->  json字符串 就可以发送出去了
    return sendBuffer;
}

// json反序列化示例1
void func5()
{               // json的反序列化
    std::string recvBuffer = func4();

    // 反序列化 -- 用parse方法                        反序列化
    json js = json::parse(recvBuffer);  // json字符串   ->   json数据对象 

    std::cout << "js['int list'] = " << js["int list"] << std::endl;
    std::cout << "js['mountains'] = " << js["mountains"] << std::endl;
}

// json反序列化示例1
void func6()
{
    std::string jsbuf = func4();
    json js = json::parse(jsbuf);

    std::map< int, std::string> map = js["mountains"];
    for( auto elem : map)
    {
        std::cout << elem.first << " " << elem.second << "\n";
    }
}


int main() 
{
    func6();
    
    return 0;
}
