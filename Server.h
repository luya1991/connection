#ifndef SERVER_H
#define SERVER_H

#include "Headers.h"

class Server
{
public:
	Server(const int listenfd = -1);
	~Server();

private:
	Server(const Server&);
	Server& operator = (const Server&);

public:
	void WaitForConnection(const int port = SERVER_PORT);

private:
	void StringServerBySelect();
	void StringServerByEpollLT();
	void StringServerByEpollET();

	inline static void SignalHandler(int signo) { m_ExitFlag = 1; }

private:
	int 	m_ListenFd;
	
	int* 	m_ClientFds;
	char** 	m_ClientsBuffers;
	int 	m_ClientCount;
	
	static volatile int m_ExitFlag;
};

#endif // SERVER_H
