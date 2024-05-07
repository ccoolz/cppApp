#include "dealclient.h"

DealClient::DealClient()
{
}

DealClient::~DealClient()
{
}

bool DealClient::Start(XTcp client_)
{
	client = client_;
	std::thread thr(&DealClient::threadFunc, this);
	thr.detach();
	return true;
}

void DealClient::threadFunc()		// ͨ���߳�
{					
	char buf[10240] = { 0 };
	bool occur_error = false;										// �����ڲ�ѭ��������ͳһ�������

	// ѭ��һ��| �������� -- �������� -- ��Ӧ���� |�Ĺ���
	for ( ; ; )
	{			// �Ƚ�����������
		int recv_len = client.receiveMsg(buf, sizeof(buf) - 1);
		buf[ recv_len] = '\0';										// ��Ȼ��ʼ��Ϊȫ'\0'����bufҪ�������ã��Ǿ�Ҫ�ֶ���'\0'�������ȡ���ݴ���
		if (recv_len <= 0) break;									// <0 �������=0����Է��ر������ӣ���֮�������ټ����ͶԷ�ͨ����
		std::cout << buf << "\n";
				// ���������ݴ��ݸ� response�������н���
		if (response.parseRequest(buf) == false) break;
				// ͨ�� response����Ľ����õ�ͷ��Ϣ
		std::string head_msg = response.getHeadMsg();
				// ����ͷ��Ϣ
		int send_len = client.sendMsg(head_msg.c_str(), head_msg.size());
		if (send_len <= 0) break;
		std::cout << "Send head message to the Browser succeeded.\n";
				// ����������������ݣ��ļ���
		int buf_size = sizeof(buf);
		
		for ( ; ; )													// �ļ����ݿ��ܴܺ�ͨ��ѭ������| ��һ����buf��С���ļ����� -- �����ļ����� |�Ĺ���ֱ�����ļ���ȡ��
		{					// ������
			int read_len = response.readMsg(buf, buf_size);
			if (read_len < 0)
			{
				occur_error = true;
				break;												// ע�⣺��ֻ�������ڲ�ѭ����Ȼ��Ҫ���д�����ʹ���
			}
			if (read_len == 0) break;								// readMsg�������Դ��Ķ����ݺ������� sendMsg �Ƿ�װ�� send()�� recvMsg ��װ�� recv()�����Ĵ����鲻ͬ --- ������<0�Ǵ�=0�ǹر����ӣ���readMsg <0�Ǵ�=0�����ݶ����ˣ���������ѭ����������������
							// ������
			int send_len = client.sendMsg(buf, read_len);
			if (send_len <= 0)
			{
				occur_error = true;
				break;
			}
		}
		if (occur_error == true) break;
		std::cout << "Transferring file data to the Browser succeeded.\n";
	}

	client.closeSock();
	delete this;			// �����������趨dealclient�����ڶ������ɣ�����ѭ����˵����������ӶϿ���Ҫ���ո���Դ
}
