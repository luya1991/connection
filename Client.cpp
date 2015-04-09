
#include "Client.h"

volatile int Client::m_ExitFlag = 0;

Client::Client(const int fd) : m_Fd(fd)
{
#ifdef USE_EPOLL_ET
	m_SendBuffer = new char[BUFFER_SIZE];
	m_RecvBuffer = new char[BUFFER_SIZE];
#else
	m_Buffer = new char[BUFFER_SIZE];
#endif

	struct sigaction act, oact;

	act.sa_handler = SignalHandler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	Sigaction(SIGINT, &act, &oact);
}

Client::~Client()
{
	if (m_Fd != -1)
	{
		Close(m_Fd);
		m_Fd = -1;
	}

#ifdef USE_EPOLL_ET
	if (m_SendBuffer)
	{
		delete [] m_SendBuffer;
		m_SendBuffer = NULL;
	}

	if (m_RecvBuffer)
	{
		delete [] m_RecvBuffer;
		m_RecvBuffer = NULL;
	}
#else
	if (m_Buffer)
	{
		delete [] m_Buffer;
		m_Buffer = NULL;
	}
#endif
}

void Client::ConnectionStartUp(const char* ipaddr, const int port)
{
	int sockfd;
	struct sockaddr_in servaddr;

	sockfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	Inet_pton(AF_INET, ipaddr, &servaddr.sin_addr);
	Connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

	m_Fd = sockfd;

#if USE_SELECT
	StringClientBySelect();
#elif USE_EPOLL_LT
	StringClientByEpollLT();
#elif USE_EPOLL_ET
	StringClientByEpollET();
#endif
}

#if USE_SELECT
void Client::StringClientBySelect()
{
	int nread, maxfdp1;
	fd_set rset, set;

	const int sockfd = m_Fd;
	char* buf = m_Buffer;

	FD_ZERO(&set);
	FD_SET(STDIN_FILENO, &set);
	FD_SET(sockfd, &set);

	maxfdp1 = (sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO) + 1;

	for (;;)
	{
		/*
		* Have to update rset every time before calling 
		* select(), because client's input may have ended
		*/

		rset = set;

		/*
		* Need to use the original select(), rather than the
		* wrapped Select(), because we need to catch signal 
		* interruptions during select() and return to main()
		* to do the clean-up job
		*/

		int nready = select(maxfdp1, &rset, NULL, NULL, NULL);

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

		if (FD_ISSET(sockfd, &rset))			/* socket is readable */
		{
			if ((nread = Read(sockfd, buf, BUFFER_SIZE)) == 0)
			{
				if (m_ExitFlag == 1)
					return;			/* normal termination */
				else
				{
					std::cout << "server terminated prematurely" << std::endl;
					return;			/* server break down */
				}
			}

			Write(STDOUT_FILENO, buf, nread);
		}

		if (FD_ISSET(STDIN_FILENO, &rset))		/* stdin is readable */
		{
			if ( (nread = Read(STDIN_FILENO, buf, BUFFER_SIZE)) == 0)
			{
				Shutdown(sockfd, SHUT_WR);
				FD_CLR(STDIN_FILENO, &set);
				continue;			/* end of client's input */
			}

			Write(sockfd, buf, nread);
		}
	}
}
#endif

#if USE_EPOLL_LT
void Client::StringClientByEpollLT()
{
	int i, nread, epollfd;
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];

	const int sockfd = m_Fd;
	char* buf = m_Buffer;

	epollfd = Epoll_create(MAX_EVENTS);

	memset(&ev, 0, sizeof(struct epoll_event));
	memset(events, 0, sizeof(struct epoll_event) * MAX_EVENTS);

	ev.data.fd = sockfd;
	ev.events = EPOLLIN;
	Epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);

	ev.data.fd = STDIN_FILENO;
	ev.events = EPOLLIN;
	Epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

	for (;;)
	{
		/*
		* Need to use the original epoll_wait(), rather
		* than the wrapped Epoll_wait(), the same reason
		* as select() above
		*/

		int nready = epoll_wait(epollfd, events, MAX_EVENTS, -1);

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
			if (events[i].data.fd == sockfd)	/* socket is readable*/
			{
				if ((nread = Read(sockfd, buf, BUFFER_SIZE)) == 0)
				{
					if (m_ExitFlag == 1)
						return;		/* normal termination */

					else
					{
						std::cout << "server terminated prematurely" << std::endl;
						return; 	/* server break down */
					}
				}

				Write(STDOUT_FILENO, buf, nread);
			}

			else if (events[i].data.fd == STDIN_FILENO)	/* stdin is readable */
			{
				if ((nread = Read(STDIN_FILENO, buf, BUFFER_SIZE)) == 0)
				{
					Shutdown(sockfd, SHUT_WR);
					Epoll_ctl(epollfd, EPOLL_CTL_DEL, STDIN_FILENO, &ev);
					continue;			/* end of client's input */
				}

				Write(sockfd, buf, nread);
			}
		}
	}

	// exiting
	Close(epollfd);
}
#endif

#if USE_EPOLL_ET
void Client::StringClientByEpollET()
{
	int i, nread, nwrite, epollfd;
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];

	const int sockfd = m_Fd;
	char* sendbuf = m_SendBuffer;
	char* recvbuf = m_RecvBuffer;

	// pointers for buffer management
	char *psendi, *psendo, *precvi, *precvo;

	psendi = psendo = sendbuf;
	precvi = precvo = recvbuf;

	epollfd = Epoll_create(MAX_EVENTS);

	memset(&ev, 0, sizeof(struct epoll_event));
	memset(events, 0, sizeof(struct epoll_event) * MAX_EVENTS);

	ev.data.fd = sockfd;
	ev.events = EPOLLIN | EPOLLET;
	SetNonBlocking(sockfd);
	Epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);

	ev.data.fd = STDIN_FILENO;
	ev.events = EPOLLIN | EPOLLET;
	SetNonBlocking(STDIN_FILENO);
	Epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

	for (;;)
	{
		/*
		* Need to use the original epoll_wait(), rather
		* than the wrapped Epoll_wait(), the same reason
		* as select() above
		*/

		int nready = epoll_wait(epollfd, events, MAX_EVENTS, -1);

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
			// read event on stdin
			if (events[i].data.fd == STDIN_FILENO && events[i].events & EPOLLIN)
			{
				// need to use the original 'read' to handle EWOULDBLOCK
				if ((nread = read(STDIN_FILENO, psendi, (sendbuf + BUFFER_SIZE - psendi))) < 0)
				{
					if (errno != EWOULDBLOCK || errno != EINTR)
					{
						std::cout << "read stdin error: ";
						std::cout << strerror(errno) << std::endl;

						// terminate
						return;
					}
				}

				// handle 'EOF' on stdin
				else if (nread == 0)
				{
					if (psendi == psendo)
					{
						Shutdown(sockfd, SHUT_WR);
						Epoll_ctl(epollfd, EPOLL_CTL_DEL, STDIN_FILENO, &ev);
						continue;
					}
				}

				// normal state
				else
				{
					psendi += nread;

					ev.data.fd = sockfd;
					ev.events = EPOLLOUT | EPOLLET;
					SetNonBlocking(sockfd);
					Epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev);
				}
			}

			// read event on sockfd
			if (events[i].data.fd == sockfd && events[i].events & EPOLLIN)
			{
				// need to use the original 'read' to handle EWOULDBLOCK
				if ((nread = read(sockfd, precvi, (recvbuf + BUFFER_SIZE - precvi))) < 0)
				{
					if (errno != EWOULDBLOCK || errno != EINTR)
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
					if (m_ExitFlag == 1)
						return;

					else
					{
						std::cout << "server terminated prematurely" << std::endl;
						return; 	/* server break down */
					}
				}

				// normal state
				else
				{
					precvi += nread;

					ev.data.fd = STDOUT_FILENO;
					ev.events = EPOLLOUT | EPOLLET;
					SetNonBlocking(STDOUT_FILENO);
					Epoll_ctl(epollfd, EPOLL_CTL_ADD, STDOUT_FILENO, &ev);
				}
			}

			// write event on socket
			if (events[i].data.fd == sockfd && events[i].events & EPOLLOUT
			    && (psendi - psendo > 0))
			{
				int bytestowrite = psendi - psendo;

				// need to use the original 'write' to handle EWOULDBLOCK
				if ((nwrite = write(sockfd, psendo, bytestowrite)) < 0)
				{
					if (errno != EWOULDBLOCK || errno != EINTR)
					{
						std::cout << "write socket error: ";
						std::cout << strerror(errno) << std::endl;

						// terminate
						return;
					}
				}

				else
				{
					psendo += nwrite;

					if (psendo == psendi)
					{
						psendi = psendo = sendbuf;

						ev.data.fd = sockfd;
						ev.events = EPOLLIN | EPOLLET;
						SetNonBlocking(sockfd);
						Epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev);
					}
				}
			}

			// write event on stdout
			if (events[i].data.fd == STDOUT_FILENO && events[i].events & EPOLLOUT
			    && (precvi - precvo > 0))
			{
				int bytestowrite = precvi - precvo;

				// need to use the original 'write' to handle EWOULDBLOCK
				if ((nwrite = write(STDOUT_FILENO, precvo, bytestowrite)) < 0)
				{
					if (errno != EWOULDBLOCK || errno != EINTR)
					{
						std::cout << "write stdout error: ";
						std::cout << strerror(errno) << std::endl;

						// terminate
						return;
					}
				}

				else
				{
					precvo += nwrite;

					if (precvo == precvi)
					{
						precvi = precvo = recvbuf;
						Epoll_ctl(epollfd, EPOLL_CTL_DEL, STDOUT_FILENO, &ev);
					}
				}
			}
		}
	}

	// exiting
	Close(epollfd);
}
#endif
