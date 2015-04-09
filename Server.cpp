
#include "Server.h"

volatile int Server::m_ExitFlag = 0;

Server::Server(const int listenfd)
    : m_ListenFd(listenfd), m_ClientCount(0)
{
	int i;

	m_ClientFds = new int[MAX_EVENTS];
	
	for (i = 0; i < MAX_EVENTS; i++)
		m_ClientFds[i] = -1;

	m_ClientsBuffers = new char*[MAX_EVENTS];
	
	for (i = 0; i < MAX_EVENTS; i++)
		m_ClientsBuffers[i] = NULL;

	struct sigaction act, oact;

	act.sa_handler = SignalHandler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	Sigaction(SIGINT, &act, &oact);
}

Server::~Server()
{
	if (m_ListenFd != -1)
	{
		Close(m_ListenFd);
		m_ListenFd = -1;
	}

	int i;

	for (i = 0; i < MAX_EVENTS; i++)
	{
		if (m_ClientFds[i] != -1)
		{
			Close(m_ClientFds[i]);
			m_ClientFds[i] = -1;
		}

		if (m_ClientsBuffers[i] != NULL)
		{
			delete [] m_ClientsBuffers[i];
			m_ClientsBuffers[i] = NULL;
		}
	}

	delete [] m_ClientFds;
	delete [] m_ClientsBuffers;
}

void Server::WaitForConnection(const int port)
{
	int listenfd;
	struct sockaddr_in servaddr;

	listenfd = Socket(AF_INET, SOCK_STREAM, 0);

	const int on = 1;
	Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	Bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	Listen(listenfd, LISTEN_QUEUE);

	m_ListenFd = listenfd;

#if USE_SELECT
	StringServerBySelect();
#elif USE_EPOLL_LT
	StringServerByEpollLT();
#elif USE_EPOLL_ET
	StringServerByEpollET();
#endif
}

#if USE_SELECT
void Server::StringServerBySelect()
{
	fd_set rset, set;
	int i, connfd, maxfdp1, nready, maxindex;

	const int listenfd = m_ListenFd;

	maxindex = -1;
	maxfdp1 = listenfd + 1;
	FD_ZERO(&set);
	FD_SET(listenfd, &set);

	for (;;)
	{
		/*
		* Have to update rset every time before
		* calling select(), because some clients may
		* have closed their connection descriptor already
		*/

		rset = set;

		/*
		* Need to use the original select(), rather than the
		* wrapped Select(), because we need to catch signal 
		* interruptions during select() and return to main()
		* to do the clean-up job
		*/

		nready = select(maxfdp1, &rset, NULL, NULL, NULL);

		if ((nready == -1) && (errno == EINTR) && (m_ExitFlag == 1))
			break;

		else if ((nready == -1) && (errno == EINTR) && (m_ExitFlag == 0))
			continue;

		else if (nready == -1)
		{
			std::cout << "select error: ";
			std::cout << strerror(errno) << std::endl;
			exit(-1);
		}

		/* new connection request */
		if (FD_ISSET(listenfd, &rset))
		{
			sockaddr_in cliaddr;
			socklen_t clilen = sizeof(cliaddr);

			connfd = Accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);

			std::cout << "a connection has been established on fd: " << connfd << std::endl;

			if (m_ClientCount >= MAX_EVENTS)
			{
				std::cout << "too many clients" << std::endl;
				Close(connfd);
				continue;
			}

			for (i = 0; i < MAX_EVENTS; i++)
			{
				if (m_ClientFds[i] == -1)
				{
					m_ClientFds[i] = connfd;
					m_ClientsBuffers[i] = new char[BUFFER_SIZE];
					m_ClientCount++;
					break;
				}
			}

			if (i > maxindex)
				maxindex = i;

			FD_SET(connfd, &set);		/* add new descriptor to set */

			if (connfd + 1 > maxfdp1)
				maxfdp1 = connfd + 1;	/* update maxfdp1 */

			if (--nready <= 0)
				continue;               /* no more readable fds */
		}

		char* buf;
		int sockfd;
		ssize_t nread;

		/* check all clients for data */
		for (i = 0; i <= maxindex; i++)
		{
			if (m_ClientFds[i] == -1)
				continue;

			sockfd = m_ClientFds[i];

			if (FD_ISSET(sockfd, &rset))
			{
				buf = m_ClientsBuffers[i];

				if ((nread = Read(sockfd, buf, BUFFER_SIZE)) == 0)
				{
					FD_CLR(sockfd, &set);
					std::cout << "client has closed connection" << std::endl;
					Close(m_ClientFds[i]);
					m_ClientFds[i] = -1;
					delete [] m_ClientsBuffers[i];
					m_ClientsBuffers[i] = NULL;
					m_ClientCount--;
				}

				else if(nread > 0)
					Write(sockfd, buf, nread);

				if (--nready <= 0)
					break;		/* no more readable fds */
			}
		}
	}
}
#endif

#if USE_EPOLL_LT
void Server::StringServerByEpollLT()
{
	int connfd, epollfd, maxindex;
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];

	maxindex = -1;
	const int listenfd = m_ListenFd;

	epollfd = Epoll_create(MAX_EVENTS);

	memset(&ev, 0, sizeof(struct epoll_event));
	memset(events, 0, sizeof(struct epoll_event) * MAX_EVENTS);

	ev.events = EPOLLIN;
	ev.data.fd = listenfd;
	Epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev);

	for (;;)
	{
		int i, j, nready;

		/*
		* Need to use the original epoll_wait(), rather
		* than the wrapped Epoll_wait(), the same reason
		* as select() above
		*/

		nready = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		if ((nready == -1) && (errno == EINTR) && (m_ExitFlag == 1))
			break;

		else if ((nready == -1) && (errno == EINTR) && (m_ExitFlag == 0))
			continue;

		else if (nready == -1)
		{
			std::cout << "epoll_wait error: ";
			std::cout << strerror(errno) << std::endl;
			exit(-1);
		}

		for (i = 0; i < nready; i++)
		{
			/* new connection request */
			if (events[i].data.fd == listenfd)
			{
				sockaddr_in cliaddr;
				socklen_t clilen = sizeof(cliaddr);

				connfd = Accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);

				std::cout << "a connection has been established on fd: " << connfd << std::endl;

				if (m_ClientCount >= MAX_EVENTS)
				{
					std::cout << "too many clients" << std::endl;
					Close(connfd);
					continue;
				}

				for (j = 0; j < MAX_EVENTS; j++)
				{
					if (m_ClientFds[j] == -1)
					{
						m_ClientFds[j] = connfd;
						m_ClientsBuffers[j] = new char[BUFFER_SIZE];
						m_ClientCount++;
						break;
					}
				}

				if (j > maxindex)
					maxindex = j;

				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = connfd;
				Epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev);
			}

			else
			{
				char* buf;
				int sockfd;
				ssize_t nread;

				/* check all clients for data */
				for (j = 0; j <= maxindex; j++)
				{
					if (m_ClientFds[j] == -1)
						continue;

					sockfd = m_ClientFds[j];

					if (sockfd == events[i].data.fd)
					{
						buf = m_ClientsBuffers[j];
						break;
					}
				}

				if ((nread = Read(sockfd, buf, BUFFER_SIZE)) == 0)
				{
					Epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &ev);
					std::cout << "client has closed connection" << std::endl;
					Close(m_ClientFds[j]);
					m_ClientFds[j] = -1;
					delete [] m_ClientsBuffers[j];
					m_ClientsBuffers[j] = NULL;
					m_ClientCount--;
				}

				else if(nread > 0)
					Write(sockfd, buf, nread);
			}
		}
	}

	// exiting
	Close(epollfd);
}
#endif

#if USE_EPOLL_ET
void Server::StringServerByEpollET()
{
	int connfd, epollfd, maxindex;
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];

	maxindex = -1;
	const int listenfd = m_ListenFd;

	epollfd = Epoll_create(MAX_EVENTS);

	memset(&ev, 0, sizeof(struct epoll_event));
	memset(events, 0, sizeof(struct epoll_event) * MAX_EVENTS);

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = listenfd;
	SetNonBlocking(listenfd);
	Epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev);

	for (;;)
	{
		int i, j, nready;

		/*
		* Need to use the original epoll_wait(), rather
		* than the wrapped Epoll_wait(), the same reason
		* as select() above
		*/

		nready = epoll_wait(epollfd, events, MAX_EVENTS, -1);

		if ((nready == -1) && (errno == EINTR) && (m_ExitFlag == 1))
			break;

		else if ((nready == -1) && (errno == EINTR) && (m_ExitFlag == 0))
			continue;

		else if (nready == -1)
		{
			std::cout << "epoll_wait error: ";
			std::cout << strerror(errno) << std::endl;
			exit(-1);
		}

		for (i = 0; i < nready; i++)
		{
			/* new connection request */
			if (events[i].data.fd == listenfd && events[i].events & EPOLLIN)
			{
				sockaddr_in cliaddr;
				socklen_t clilen = sizeof(cliaddr);

				// need to use the original 'accept' to handle EWOULDBLOCK
				if ((connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen)) < 0)
				{
					if (connfd == EWOULDBLOCK)
						continue;

					else
					{
						std::cout << "accept error: ";
						std::cout << strerror(errno) << std::endl;

						return;
					}
				}

				std::cout << "a connection has been established on fd: " << connfd << std::endl;

				if (m_ClientCount >= MAX_EVENTS)
				{
					std::cout << "too many clients" << std::endl;
					Close(connfd);
					continue;
				}

				for (j = 0; j < MAX_EVENTS; j++)
				{
					if (m_ClientFds[j] == -1)
					{
						m_ClientFds[j] = connfd;
						m_ClientsBuffers[j] = new char[BUFFER_SIZE];
						m_ClientCount++;
						break;
					}
				}

				if (j > maxindex)
					maxindex = j;

				ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
				ev.data.fd = connfd;
				SetNonBlocking(connfd);
				Epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev);
			}

			else
			{
				char* buf;
				int sockfd;
				ssize_t nread, nwrite;
				
				// pointers for buffer control
				char *pbufi, *pbufo;

				/* check all clients for data */
				for (j = 0; j <= maxindex; j++)
				{
					if (m_ClientFds[j] == -1)
						continue;

					sockfd = m_ClientFds[j];

					if (sockfd == events[i].data.fd)
					{
						buf = m_ClientsBuffers[j];
						pbufi = pbufo = buf;
						break;
					}
				}

				// read event on client[j]
				if (events[i].events & EPOLLIN)
				{
					// need to use the original 'read' to handle EWOULDBLOCK
					if ((nread = read(sockfd, pbufi, (buf + BUFFER_SIZE - pbufi))) < 0)
					{
						if (errno != EWOULDBLOCK)
						{
							std::cout << "read socket error: ";
							std::cout << strerror(errno) << std::endl;

							// terminate
							return;
						}
					}

					// handle 'EOF' on sockfd
					else if (nread == 0)
					{
						Epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &ev);
						std::cout << "client has closed connection" << std::endl;
						Close(m_ClientFds[j]);
						m_ClientFds[j] = -1;
						delete [] m_ClientsBuffers[j];
						m_ClientsBuffers[j] = NULL;
						m_ClientCount--;
					}

					// normal state
					else
						pbufi += nread;
				}

				// write event on client[j]
				if (events[i].events & EPOLLOUT && (pbufi - pbufo > 0))
				{
					int bytestowrite = pbufi - pbufo;

					// need to use the original 'write' to handle EWOULDBLOCK
					if ((nwrite = write(sockfd, pbufo, bytestowrite)) < 0)
					{
						if (errno != EWOULDBLOCK)
						{
							std::cout << "write socket error: ";
							std::cout << strerror(errno) << std::endl;

							// terminate
							return;
						}
					}

					else
					{
						pbufo += nwrite;

						if(pbufo == pbufi)
							pbufi = pbufo = buf;
					}
				}
			}
		}
	}

	// exiting
	Close(epollfd);
}
#endif
