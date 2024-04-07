#include "xtcp.h"
XTcp::XTcp()
{
#ifdef _WIN32
	static bool first = true;
	if (first == true)			//  þ ̬    ʹWindosw ϲ    ظ   ʼ    ̬   ӿ 
	{							//   ̬    ֻ   ʼ  һ  
		first = false;
		WSADATA ws;
		WSAStartup(MAKEWORD(2, 2), &ws);
	}
#endif
}
XTcp::~XTcp()
{
}
int XTcp::createSocket()
{
	//     socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		std::cout << "CREATE SOKCET FAILED!\n";
	}
	else
	{
		std::cout << "CREATE SOKCET SUCCEEDED!";
	}
	return sock;
}
bool XTcp::bindPort(unsigned short port_)
{
	if (sock <= 0)
		createSocket();
	//             ַ ṹ  
	sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port_);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//  󶨼   socket ͷ       ַ
	int bind_res = bind(sock, (sockaddr*)(&saddr), sizeof(saddr));
	if (bind_res != 0)		//  󶨳ɹ  ᷵  0
	{
		printf("bind port %d failed!\n", port_);
		return false;		//   ʧ  
	}
	printf("bind port %d succeeded!\n", port_);
	//   ʼ      
	listen(sock, 10);
	return true;			//  󶨳ɹ 
}
XTcp XTcp::acceptRequst()
{
	XTcp client;
	sockaddr_in caddr;
	socklen_t len_caddr = sizeof(caddr);							//    int   屨    VS   accept()            int*  g++     socklen_t Ҳ     unsigned int
	client.sock = accept(this->sock, (sockaddr*)(&caddr), &len_caddr);
	if (client.sock == -1)
	{
		std::cout << "CREATE SOKCET FAILED!\n";
		return client;
	}
	/* accept    ʱ         ˵ĳ    һֱ        һ   ͻ    򷢳      ӡ
		accept ɹ ʱ       ķ      ˵  ļ            ʱ        ˿            д  Ϣ ˣ ʧ  ʱ     - 1   */
	client.ip = inet_ntoa(caddr.sin_addr);
	client.port = caddr.sin_port;
	printf("accepted connection request from client %d.\n", client.sock);
	std::cout << "client [ip] -- " << client.ip << ", client [port] -- " << client.port << ".\n";
	return client;
}
void XTcp::closeSock()
{
	if (this->sock == -1) return;	//   socket        close       κδ   
#if _WIN32
	closesocket(sock);
#else
	close(sock);
#endif
}
int XTcp::receiveMsg(char* buf, int buf_size)
{
	return recv(sock, buf, buf_size, 0);
}
int XTcp::sendMsg(const char* msg, int msg_size)
{						//  ӷ      ĽǶȣ     Ҫ  ֤  Ϣ         ͳ ȥ
	int sended_size = 0;				//  ѷ     Ϣ ĳ   
	while (sended_size != msg_size)		//    ѷ  ͳ  Ȳ     Ҫ   ĳ  ȣ   һֱ  
	{
		int this_size = send(sock, msg + sended_size, msg_size - sended_size, 0);
		if (this_size <= 0) break;		//    쳣״  ֱ   ˳      ⷢ    ѭ  
		sended_size += this_size;
	}
	return sended_size;
}
int XTcp::getSock()
{
	return this->sock;
}
bool XTcp::connectServer(const char* ip_, unsigned short port_, int timeout_ms)
{
	if (sock <= 0) createSocket();
	sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port_);
	saddr.sin_addr.s_addr = inet_addr(ip_);
	//  unsigned long inet_addr(const char *cp);      ȷִ н     һ   ޷  ų                    ַ       һ   Ϸ   IP  ַ        INADDR_NONE  
	//  ͻ  ˵ socket  Ŀ   ַsaddr        
	// ΪʲôҪ  select   ->   ʱ     ӻ   Ϊ             ü ʮ 룬        Ǻܴ ģ    ǣ      ֲ Ҫ     Ӳ   ֱ ӷ  ء   ô     ǿ     select   пɽ   ʱ   ڵ                 ʱ 䣬˵           ⣬   ز   ӡ    select   Զ Ҫ   ӵĿͻ   socket   м         ֻ       Ŀ д Ƿ       һ          ˵     ӳɹ  ˣ    ӳɹ  ˾Ϳ     socket  д  Ϣ ˣ             趨      ʱ 䣬˵      ʧ    
	this->setBlock(false);	//   Ϊ      
	fd_set f_set;			// Ҫ   ӵ  ļ           
	int connect_res = connect(sock, (sockaddr*)(&saddr), sizeof(saddr));
	if (connect_res != 0)	//    ӳɹ     0
	{
		FD_ZERO(&f_set);			// f_set  ÿ 
		FD_SET(sock, &f_set);		//   sock ӵ       Ա㱻select()    
		timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 1000 * timeout_ms;						// ΢  ת   룬*1000
		if (select(sock + 1, 0, &f_set, 0, &tv) <= 0)		//   һ        set   ļ            ֵ+1  Windows      ν  Linux  һ  Ҫ׼ȷ   ڶ            ǣ Ҫ      ( ɶ     д ǲ  ǲ       )   ļ          ϣ    ĸ        ļ  Ƿ    쳣                 ʱ 䣬    timeval ṹ 壬    ʱ 仹û   κα      ļ       Ӧ 仯   ͻ᷵  0             źŻ᷵  -1       ļ ׼     ˣ      д   ٱ                   socketֵ    ΪNULL       ޵ȴ ֱ                
		{
			printf("connection timeout or error!\n");
			printf("connect %s:%d failed!: %s\n", ip_, port_, strerror(errno));
			return false;
		}
		//      ڼ    ӳɹ  ˣ               if     »ص     ģʽ   еĺ     Ȼ  Ҫ          accept ȣ     ӡ ɹ   Ϣ      true
	}
	this->setBlock(true);
	printf("connect %s:%d successfully!\n", ip_, port_);
	return true;
}
//    ֪       û                   socket    TCP   ӵ ʱ  Ҳ   ǵ   connectʱ    
//  һ     粻ͨ        ip  ַ  Ч   Ϳ   ʹ     ߳       һ  Ϊ30 루 Ҳ    20 룩  
//         Ϊ      ģʽ   ܺܺõĽ        ⣬    ͨ          (  )    ģʽ  
bool XTcp::setBlock(bool is_block)		//  л      ͷ            Ҫ       ÿͻ  ˵ connect   󲻻   Ϊ               ʱ 䣬       ͻ         
{
	if (sock <= 0) return false;    // sock ô  ڣ                
#ifdef _WIN32   // windows  ʹ  ioctlsocket()    ʵ  
	unsigned long cmd_arg = 0;              //     ģʽ    ֵΪ0
	if (is_block == false) cmd_arg = 1;     //  û       Ƿ     ģʽ
	ioctlsocket(sock, FIONBIO, &cmd_arg);  //  ڶ       Ϊ         sock  ĳ                Ǵ        Ϊ    /                     Ϊ        Ĳ   
#else
	int flags = fcntl(sock, F_GETFL, 0);    // fcntl     ļ        ĺ     F_GETFL    ļ    ԣ     flags  flags  ÿһλ   в ͬ     Ժ   
	if (flags < 0) return false;             // < 0       
	if (is_block == true)
	{
		flags = flags & ~O_NONBLOCK;        // O_NONBLOCK  Linux е һ  ֵ      ĳһλ        Ϊ1        Ϊ0  
	}                                       // O_NONBLOCKȡ          λΪ0      Ϊ1    flags 룬  flags      λΪ0      λ   ı䣬 Ӷ  ﵽ      
	else
	{
		flags = flags | O_NONBLOCK;         //          ôflags      λһ    1      λ    
	}
	if (fcntl(sock, F_SETFL, flags) != 0)
		return false;                       //    ı    ļ     ֵ   sock        0˵      ʧ  
#endif
	return true;
}
// ----------------------------------------------------------
void HttpThread::setClient(XTcp client_)
{
	client = client_;
}
void HttpThread::threadFunc()
{								// 进入线程函数，说明本服务器和浏览器客户端已经连接上了
	// 连接后，线程函数做的事情：接收请求 - 解析请求 - 响应请求（发送消息头 - 发送请求的html文件数据）
	char buf[10000];
	for (; ; )		// 防止短连接（一次连接只进行一次数据收发），在连接后用循环处理数据即可
	{
		// 1.接收http客户端请求信息
		int recv_size = client.receiveMsg(buf, sizeof(buf));
		if (recv_size <= 0)
		{			// 失败
			client.closeSock();
			delete this;
			return;
		}
		buf[recv_size] = '\0';			// 因为重复利用buf，结尾加\0避免出错（后面转为string的时候就容易出错）
		// 成功，打印请求信息
		printf("\n==========Receive==========\n%s===========================\n", buf);
		// 2.用正则表达式解析请求信息							状态行消息一般如 GET / HTTP/1.1
		std::string src = buf;								// 源 -- 请求消息
		std::string pattern = "^([A-Z]+) (.+) HTTP/1";		// 匹配规则 -- ()表示取匹配的字符串 ^表示开始位置 +表示1到多个 *表示0到多个 .表示任意匹配
		std::regex reg(pattern);							// 传递规则到正则表达式对象
		std::smatch matches;								// 存匹配到的结果数组
		std::regex_search(src, matches, reg);				// 从源头src搜索，通过装入匹配规则的正则表达式搜索匹配到的结果，存入matches数组
		if (matches.size() == 0)	// 结果数组为0.说明没匹配到 http请求
		{
			printf("MATCH %s FAILED!", pattern.c_str());
			client.closeSock();
			delete this;
			return;
		}
		// matches[0] 中存储的应该是整个匹配到的内容，如 GET / HTTP/1（截至HTTP/1）
		std::string type = matches[1];		// 请求类型 如 GET、POST
		std::string path = matches[2];		// 请求访问的路径 如 /index.html
		if (type != "GET")
		{					// 我们只处理 GET请求
			client.closeSock();
			delete this;
			return;
		}
		std::string filename = path;
		if (path == "/")				// 若用户没有指定访问路径
		{
			filename = "/index.html";	// 设定默认的访问路径
		}
		std::string filepath = "www";
		filepath += filename;
		FILE* fp = fopen(filepath.c_str(), "rb");			// 以只读二进制方式打开目标html文件
		if (fp == nullptr)
		{					// 打开失败
			client.closeSock();
			delete this;
			return;
		}
		fseek(fp, 0, SEEK_END);		// 文件指针从开始移到结尾
		int filesize = ftell(fp);		// 然后获取文件大小
		printf("file size -- %d\n", filesize);
		char content_len[128] = { 0 };
		sprintf(content_len, "%d", filesize);		// 将数字类型的文件大小变量转为字符串类型，方便后续操作
		fseek(fp, 0, 0);				// 再移回到文件初始位置，因为之后还要读文件
		// 3.响应http GET请求 （一定要按回应格式）
		std::string respond_msg = "";
		// 状态行
		respond_msg = "HTTP/1.1 200 OK\r\n";
		// 消息报头
		respond_msg += "Server: XHttp\r\n";
		respond_msg += "Content-Type: text/html\r\n";
		respond_msg += "Content-Length: ";
		respond_msg += content_len;			// 回应内容的长度（html文件的大小）
		respond_msg += "\r\n\r\n";				// 读到两个\r\n，说明消息报头结束
		// 发送消息头给客户端
		int send_size = client.sendMsg(respond_msg.c_str(), respond_msg.size());
		// 发送操作可能失败，但因为是短连接，所以无所谓处理不处理
		printf("\nsend size of msg head : %d", send_size);		// 打印发送消息的长度
		printf("\n===========Send===========\n%s\n===========================\n", respond_msg.c_str());
		// 发送正文，也就是客户端请求访问的html文件的信息
		for (; ; )
		{
			int read_len = fread(buf, sizeof(char), sizeof(buf), fp);		// 每次读buf大小的文件流给buf缓冲区，read_len
			if (read_len <= 0) break;										// <=0就是已经读到文件末尾读取完了，或者发生了错误
			int send_ret = client.sendMsg(buf, read_len);					// 将缓冲区的数据发送给客户端
			if (send_ret <= 0) break; 										// >0表示成功，返回实际发送或接受的字节数，=0表示超时，对方主动关闭了连接过程，<0出错
		}

	}
	client.closeSock();
	delete this;
}