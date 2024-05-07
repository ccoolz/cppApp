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

void DealClient::threadFunc()		// 通信线程
{					
	char buf[10240] = { 0 };
	bool occur_error = false;										// 用于内层循环跳出后统一处理错误

	// 循环一个| 接收请求 -- 解析请求 -- 响应请求 |的过程
	for ( ; ; )
	{			// 先接收请求数据
		int recv_len = client.receiveMsg(buf, sizeof(buf) - 1);
		buf[ recv_len] = '\0';										// 虽然初始化为全'\0'但是buf要反复利用，那就要手动补'\0'，避免读取数据错误
		if (recv_len <= 0) break;									// <0 代表出错；=0代表对方关闭了连接，总之，不能再继续和对方通信了
		std::cout << buf << "\n";
				// 将请求数据传递给 response类对象进行解析
		if (response.parseRequest(buf) == false) break;
				// 通过 response对象的解析得到头消息
		std::string head_msg = response.getHeadMsg();
				// 发送头消息
		int send_len = client.sendMsg(head_msg.c_str(), head_msg.size());
		if (send_len <= 0) break;
		std::cout << "Send head message to the Browser succeeded.\n";
				// 发送请求的正文内容（文件）
		int buf_size = sizeof(buf);
		
		for ( ; ; )													// 文件内容可能很大，通过循环持续| 读一部分buf大小的文件数据 -- 发送文件数据 |的过程直到将文件读取完
		{					// 读数据
			int read_len = response.readMsg(buf, buf_size);
			if (read_len < 0)
			{
				occur_error = true;
				break;												// 注意：这只是跳出内层循环，然后要进行错误检查和处理
			}
			if (read_len == 0) break;								// readMsg是我们自创的读数据函数，与 sendMsg 是封装了 send()和 recvMsg 封装了 recv()函数的错误检查不同 --- 后两者<0是错，=0是关闭连接，而readMsg <0是错，=0是数据读完了，可以跳出循环不继续读数据了
							// 发数据
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
	delete this;			// 代码中我们设定dealclient对象在堆上生成，跳到循环外说明出错或连接断开，要回收该资源
}
