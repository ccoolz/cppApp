#ifndef SERVERRESPONSE_H
#define SERVERRESPONSE_H
#include <string>

class ServerResponse
{
public:
	ServerResponse();
	~ServerResponse();
	bool parseRequest(std::string request);		// ����������Ϣ
	std::string getHeadMsg();					// ��ȡͷ��Ϣ
	int readMsg(char* buf, int buf_size);		// һ�ζ���������

private:
	long filesize;
	FILE* fp;
};


#endif