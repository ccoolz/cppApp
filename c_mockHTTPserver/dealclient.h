#ifndef DEALCLIENT_H
#define DEALCLIENT_H
#include "xtcp.h"
#include "serverresponse.h"

class DealClient
{
public:
	DealClient();
	~DealClient();
	bool Start(XTcp client_);
	void threadFunc();

public:
	XTcp client;
	ServerResponse response;
};

#endif
