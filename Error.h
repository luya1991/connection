
#ifndef _ERROR_H_
#define _ERROR_H_

extern int Socket(int domain, int type, int protocol);
extern void Inet_pton(int family, const char* strptr, void* addrptr);
extern void Connect(int fd, const struct sockaddr *sa, socklen_t salen);
extern void Bind(int fd, const struct sockaddr *sa, socklen_t salen);
extern void Listen(int fd, int backlog);
extern int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr);

extern int Select(int maxfdp1, fd_set *rset, fd_set *wset, fd_set *eset, struct timeval *timeout);
extern ssize_t Read(int fd, void *ptr, size_t nbytes);
extern void Write(int fd, void *ptr, size_t nbytes);

extern void Close(int fd);
extern void Shutdown(int fd, int how);

extern int Epoll_create(int size);
extern void Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
extern int Epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

extern void SetNonBlocking(int fd);
extern void Setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);

extern void Sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

#endif
