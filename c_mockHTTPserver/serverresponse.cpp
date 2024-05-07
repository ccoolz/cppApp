#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS		// 它要在包含要作用的函数的头文件的上面
#endif
#include "serverresponse.h"
#include <iostream>
#include <regex>

ServerResponse::ServerResponse()
	: filesize(0)
	, fp(nullptr)
{
}

ServerResponse::~ServerResponse()
{
}

bool ServerResponse::parseRequest(std::string request)
{
	std::string src = request;
	std::string pattern = "^([A-Z]+) /([a-zA-z0-9]*([.][a-zA-z]*)?)[?]?(.*) HTTP/1";
	std::regex reg( pattern);							
	std::smatch matches;								
	std::regex_search(src, matches, reg);

	if (matches.size() == 0)									// 结果数组为0.说明没匹配到 http请求
	{
		printf("MATCH %s FAILED!", pattern.c_str());
		return false;		// 所有的失败都 return false，统一交给与处理与客户端通信的 DealClient 对象处理
	}
	
	std::string req_type = matches[1];							// 请求类型 如 GET、POST
	std::string path = "/";
	path += matches[2];											// 请求访问的路径 如 /index.html
	std::string file_type = matches[3];							// 按照左括号的顺序，取到的是第二个括号中的小括号里的内容
	if (file_type.size() > 0)									// 文件后缀名取到了.(.html) ,若有文件后缀名，用substr方法去掉.
		file_type = file_type.substr(1, file_type.size() - 1);
	std::string php_vars = matches[4];							// 给php脚本传递的n变量，也是用问好取可以没有的部分，格式如 ?id=1&name=2
	
	std::cout << "Parsing Request...\n"
		<< "request type:[" << req_type << "]\n"
		<< "file path:[" << path << "]\n"
		<< "file type:[" << file_type << "]\n"
		<< "php vars:[" << php_vars << "]\n";
	if (req_type != "GET")
	{					// 我们只处理 GET请求
		printf("Only deal with GET request!!!\n");
		return false;
	}

	std::string filename = path;
	if (path == "/")				// 若用户没有指定访问路径
	{
		filename = "/index.html";	// 设定默认的访问路径
	}
	std::string filepath = "www";
	filepath += filename;

	// 如果是php文件，利用php-cgi执行脚本对应输出.html文件| 效果: php-cgi www/index.php id=1 name=zr > www/index.php.html	| php-cgi指令解析php脚本 > 重定向 生成.html文件
	if (file_type == "php")										
	{
		std::string cmd_deal_php = "php-cgi ";
		cmd_deal_php += filepath;
		cmd_deal_php += " ";
		for (int i = 0; i < php_vars.size(); i++)
		{
			if (php_vars[i] == '&')
				php_vars[i] = ' ';
		}
		cmd_deal_php += php_vars;
		cmd_deal_php += " > ";
		filepath += ".html";
		cmd_deal_php += filepath;			
		printf("%s\n", cmd_deal_php.c_str());
		system(cmd_deal_php.c_str());
	}	// 注意！执行上述 php-cgi... 指令需要超级用户运行程序

	fp = fopen(filepath.c_str(), "rb");
	if (fp == nullptr)
	{
		printf("open file %s failed!!!\n", filepath.c_str());
		return false;
	}
	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0, 0);

	if (file_type == "php")										// 若是php脚本生成的html文件，会有php消息头，把文件指针移到消息头结束符"\r\n"后面
	{
		char c = 0;
		int msg_head = 0;
		while (c != '\r')
		{
			fread(&c, sizeof(char), sizeof(char), fp);
			msg_head++;
		}
		fseek(fp, 3, SEEK_CUR);	
		msg_head += 3;
		filesize -= msg_head;
	}
	printf("file size -- %ld\nParse Request Seucceeded!\n\n", filesize);

	return true;
}

std::string ServerResponse::getHeadMsg()
{
	std::string respond_msg = "";
	char content_len[128] = { 0 };
	sprintf(content_len, "%ld", filesize);
	// 状态行
	respond_msg = "HTTP/1.1 200 OK\r\n";
	// 消息报头
	respond_msg += "Server: XHttp\r\n";
	respond_msg += "Content-Type: text/html\r\n";
	respond_msg += "Content-Length: ";
	respond_msg += content_len;									// 回应内容的长度（html文件的大小）
	respond_msg += "\r\n\r\n";									// 读到两个\r\n，说明消息报头结束

	return respond_msg;
}

int ServerResponse::readMsg(char* buf, int buf_size)
{
	return fread(buf, sizeof(char), buf_size, fp);
}
