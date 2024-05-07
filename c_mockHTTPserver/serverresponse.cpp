#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS		// ��Ҫ�ڰ���Ҫ���õĺ�����ͷ�ļ�������
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

	if (matches.size() == 0)									// �������Ϊ0.˵��ûƥ�䵽 http����
	{
		printf("MATCH %s FAILED!", pattern.c_str());
		return false;		// ���е�ʧ�ܶ� return false��ͳһ�����봦����ͻ���ͨ�ŵ� DealClient ������
	}
	
	std::string req_type = matches[1];							// �������� �� GET��POST
	std::string path = "/";
	path += matches[2];											// ������ʵ�·�� �� /index.html
	std::string file_type = matches[3];							// ���������ŵ�˳��ȡ�����ǵڶ��������е�С�����������
	if (file_type.size() > 0)									// �ļ���׺��ȡ����.(.html) ,�����ļ���׺������substr����ȥ��.
		file_type = file_type.substr(1, file_type.size() - 1);
	std::string php_vars = matches[4];							// ��php�ű����ݵ�n������Ҳ�����ʺ�ȡ����û�еĲ��֣���ʽ�� ?id=1&name=2
	
	std::cout << "Parsing Request...\n"
		<< "request type:[" << req_type << "]\n"
		<< "file path:[" << path << "]\n"
		<< "file type:[" << file_type << "]\n"
		<< "php vars:[" << php_vars << "]\n";
	if (req_type != "GET")
	{					// ����ֻ���� GET����
		printf("Only deal with GET request!!!\n");
		return false;
	}

	std::string filename = path;
	if (path == "/")				// ���û�û��ָ������·��
	{
		filename = "/index.html";	// �趨Ĭ�ϵķ���·��
	}
	std::string filepath = "www";
	filepath += filename;

	// �����php�ļ�������php-cgiִ�нű���Ӧ���.html�ļ�| Ч��: php-cgi www/index.php id=1 name=zr > www/index.php.html	| php-cgiָ�����php�ű� > �ض��� ����.html�ļ�
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
	}	// ע�⣡ִ������ php-cgi... ָ����Ҫ�����û����г���

	fp = fopen(filepath.c_str(), "rb");
	if (fp == nullptr)
	{
		printf("open file %s failed!!!\n", filepath.c_str());
		return false;
	}
	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0, 0);

	if (file_type == "php")										// ����php�ű����ɵ�html�ļ�������php��Ϣͷ�����ļ�ָ���Ƶ���Ϣͷ������"\r\n"����
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
	// ״̬��
	respond_msg = "HTTP/1.1 200 OK\r\n";
	// ��Ϣ��ͷ
	respond_msg += "Server: XHttp\r\n";
	respond_msg += "Content-Type: text/html\r\n";
	respond_msg += "Content-Length: ";
	respond_msg += content_len;									// ��Ӧ���ݵĳ��ȣ�html�ļ��Ĵ�С��
	respond_msg += "\r\n\r\n";									// ��������\r\n��˵����Ϣ��ͷ����

	return respond_msg;
}

int ServerResponse::readMsg(char* buf, int buf_size)
{
	return fread(buf, sizeof(char), buf_size, fp);
}
