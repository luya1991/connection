
#include "Headers.h"

using namespace std;

int Socket(int domain, int type, int protocol)
{
	int n;

	if ((n = socket(domain, type, protocol)) < 0)
	{
		cout << "socket error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}

	return n;
}

void Inet_pton(int family, const char* strptr, void* addrptr)
{
	int n;

	if ((n = inet_pton(family, strptr, addrptr)) < 0)
	{
		cout << "inet_pton error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}

	else if (n == 0)
	{
		cout << "inet_pton(0): invalid addr form";
		exit(-1);
	}
}

void Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
	int n;

again:	if ((n = connect(fd, sa, salen)) < 0)
	{
		if (errno == EINTR)
			goto again;

		else
		{
			cout << "connect error: ";
			cout << strerror(errno) << endl;
			exit(-1);
		}
	}
}

void Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{
	if (bind(fd, sa, salen) < 0)
	{
		cout << "bind error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}
}

void Listen(int fd, int backlog)
{
	if (listen(fd, backlog) < 0)
	{
		cout << "listen error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}
}

int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr)
{
	int n;

again:	if ((n = accept(fd, sa, salenptr)) < 0) 
	{
#ifdef	EPROTO
		if (errno == EPROTO || errno == ECONNABORTED || errno == EINTR)
#else
		if (errno == ECONNABORTED || errno == EINTR)
#endif
			goto again;
		else
		{
			cout << "accept error: ";
			cout << strerror(errno) << endl;
			exit(-1);
		}
	}

	return n;
}

int Select(int maxfdp1, fd_set *rset,  fd_set *wset, fd_set *eset, struct timeval *timeout)
{
	int n;

again:	if ((n = select(maxfdp1, rset, wset, eset, timeout)) < 0) 
	{
		if (errno == EINTR)
			goto again;

		else
		{
			cout << "select error: ";
			cout << strerror(errno) << endl;
			exit(-1);
		}
	}

	return n;		/* can return 0 on timeout */
}

ssize_t Read(int fd, void *ptr, size_t nbytes)
{
	ssize_t	n;

again:	if ((n = read(fd, ptr, nbytes)) < 0)
	{
		if (errno == EINTR)
			goto again;

		else
		{
			cout << "read error: ";
			cout << strerror(errno) << endl;
			exit(-1);
		}
	}

	return n;
}

void Write(int fd, void *ptr, size_t nbytes)
{
	ssize_t n;

again:	if ((n = write(fd, ptr, nbytes)) < 0)
	{
		if (errno == EINTR)
			goto again;

		else
		{
			cout << "write error: ";
			cout << strerror(errno) << endl;
			exit(-1);
		}
	}
}

void Close(int fd)
{
	if (close(fd) < 0)
	{
		cout << "close error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}
}

void Shutdown(int fd, int how)
{
	if (shutdown(fd, how) < 0)
	{
		cout << "shutdown error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}
}

int Epoll_create(int size)
{
	int n;

	if ((n = epoll_create(size)) < 0)
	{
		cout << "epoll_create error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}

	return n;
}

void Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	if (epoll_ctl(epfd, op, fd, event) < 0)
	{
		cout << "epoll_ctl error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}
}

int Epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	int n;

again:	if ((n = epoll_wait(epfd, events, maxevents, timeout) < 0))
	{
		if (errno == EINTR)
			goto again;

		else
		{
			cout << "epoll_wait error: ";
			cout << strerror(errno) << endl;
			exit(-1);
		}
	}

	return n;
}

void SetNonBlocking(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
	{
		cout << "fcntl(F_GETFL) error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}

	flags |= O_NONBLOCK;

	if (fcntl(fd, F_SETFL, flags) < 0)
	{
		cout << "fcntl(F_SETFL) error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}
}

void Setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	if (setsockopt(fd, level, optname, optval, optlen) < 0)
	{
		cout << "setsockopt error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}
}

void Sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	if (sigaction(signum, act, oldact) < 0)
	{
		cout << "sigaction error: ";
		cout << strerror(errno) << endl;
		exit(-1);
	}	
}
