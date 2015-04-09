
#ifndef _HEADERS_H_
#define _HEADERS_H_

#include <iostream>

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#define LISTEN_QUEUE    128
#define BUFFER_SIZE     4096

#define MAX_EVENTS      1024

#define SERVER_PORT     10086
#define SERVER_ADDR     "127.0.0.1"

// 3 options are all available
#define USE_SELECT      0

#if (!USE_SELECT)
#define USE_EPOLL_LT    0
#endif

#if ((!USE_SELECT) && (!USE_EPOLL_LT))
#define USE_EPOLL_ET    1
#endif

#include "Error.h"
#include "Client.h"
#include "Server.h"

#endif
