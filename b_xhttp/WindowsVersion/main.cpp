// �����������ʵ��һ�� ����httpЭ��ķ�����
// ��������Ҫ�� xtcp���е� TcpThread���޸�Ϊ HttpThread��
#include "xtcp.h"
int main(int argc, char* argv[])
{
	unsigned short port = 8080;
	if (argc > 1) port = atoi(argv[1]);
	XTcp server;
	server.createSocket();
	bool bind_res = server.bindPort(port);
	if (bind_res == false) return 0;
	for (;;)
	{
		// ����һ��http�ͻ�������
		XTcp client = server.acceptRequst();

		// ����socket����http�߳��࣬���Ӻ�Ĳ�������http�߳�����̺߳�������
		HttpThread* tcp_thr = new HttpThread;
		tcp_thr->setClient(client);
		std::thread thr(&HttpThread::threadFunc, tcp_thr);
		thr.detach();
	}
	server.closeSock();
	getchar();
	return 0;
}
