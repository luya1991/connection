#ifndef CLIENT_H
#define CLIENT_H

#include "Headers.h"

class Client
{
public:
	Client(const int fd = -1);
	~Client();

private:
	Client(const Client&);
	Client& operator = (const Client&);

public:
	void ConnectionStartUp
	(const char* ipaddr = SERVER_ADDR, const int port = SERVER_PORT);

private:
	void StringClientBySelect();
	void StringClientByEpollLT();
	void StringClientByEpollET();

	inline static void SignalHandler(int signo) { m_ExitFlag = 1; }

private:
	int 	m_Fd;

	#ifdef USE_EPOLL_ET
	char*	m_SendBuffer;
	char*	m_RecvBuffer;
	#else
	char* 	m_Buffer;
	#endif

	static volatile int m_ExitFlag;
};
#endif // CLIENT_H
