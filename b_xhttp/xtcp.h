// ��TCPͨ��ģ���װ��ʹ�û��������ʹ��socket���
#ifndef _XTCP_H
#define _XTCP_H

#ifdef _WIN32	// Linux�£������Windows�겻���ã����ǽ����ĺ��嶨��һ�� -----------------------|

#ifdef MY5CREATXSOCKETDLL_EXPORTS
#define MY5CREATXSOCKETDLL_API __declspec(dllexport)
#else
#define MY5CREATXSOCKETDLL_API __declspec(dllimport)
#endif

#else			// ����Windows�£��ͽ��������ݶ���Ϊ�գ����Ͳ������Ӱ���� 
#define MY5CREATXSOCKETDLL_API				// ���Ӻ궨������ݣ�������Ϊ��
#endif			// ------------------------------------------------------------------------------|

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#include <WS2tcpip.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>			// fcntl�����ļ�����
#endif
#include <thread>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <string>
#include <regex>			// C++自带的正则表达式

class MY5CREATXSOCKETDLL_API XTcp
{
public:
	XTcp();
	virtual ~XTcp();
	int createSocket();			// ��ʼ������socket��ֵ���ظ���Ա����server
	bool bindPort(unsigned short port_);
	XTcp acceptRequst();
	void closeSock();
	int receiveMsg(char* buf, int buf_size);
	int sendMsg(const char* msg, int msg_size);
	int getSock();
	bool connectServer(const char* ip_, unsigned short port_, int timeout_ms = 1000);		// Ϊ�ͻ������ӷ�װconnect�ĺ���
	bool setBlock(bool is_block);		// �л������ͷ�����

public:
	int sock = 0;				// socket��client�еľ���ͨ��socket��server�еľ��Ǽ���socket
	unsigned short port = 0;
	std::string ip;
};

// ------------------------
class MY5CREATXSOCKETDLL_API HttpThread
{
public:
	void setClient(XTcp client_);

	void threadFunc();

private:
	XTcp client;
};

#endif
