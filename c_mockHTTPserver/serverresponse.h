#ifndef SERVERRESPONSE_H
#define SERVERRESPONSE_H
#include <string>

class ServerResponse
{
public:
	ServerResponse();
	~ServerResponse();
	bool parseRequest(std::string request);		// 解析请求消息
	std::string getHeadMsg();					// 获取头消息
	int readMsg(char* buf, int buf_size);		// 一次读多少数据

private:
	long filesize;
	FILE* fp;
};


#endif