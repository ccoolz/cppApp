#include "httpserver.h"
#include "dealclient.h"

HttpServer::HttpServer()
	: is_stop(false)
{
}

HttpServer::~HttpServer()
{
}

bool HttpServer::Start(unsigned short port_)
{
	// ����HTTP���������󶨶˿�
	server.createSocket();
	bool bind_res = server.bindPort(port_);
	if( bind_res == false) return false;

	// �������̺߳����������������		��ģ��: һ�������������ж��������[����������߳�]��һ���������߳��ֿ����ж������[�������ͨ���߳�]��
	std::thread thr(&HttpServer::threadFunc, this);
	thr.detach();
	return true;
}

void HttpServer::Stop()
{
	is_stop = true;
}

void HttpServer::threadFunc()
{
	while (is_stop == false)
	{
		XTcp client = server.acceptRequst();
		DealClient* deal_client = new DealClient;		// ������������: ��DealClient���̺߳����������delete this�Լ�ɾ��
		deal_client->Start(client);
	}
}
