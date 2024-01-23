#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#if (LUA_VERSION_NUM < 502 && !defined(luaL_newlib))
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

/**
 * #define ENABLE_SOCK_DEBUG
 */

#ifdef ENABLE_SOCK_DEBUG
# define LCS_DLOG(fmt, ...) fprintf(stderr, "<lcs>" fmt "\n", ##__VA_ARGS__)
#else
# define LCS_DLOG(...)
#endif


#define LCS_CLIENT "lcsock{client}"

#if (defined(WIN32) || defined(_WIN32))

#define OS_WINDOWS
#define LAST_ERROR() (WSAGetLastError())

static void lcs_startup()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		fprintf(stderr, "WSAStartup failed with error: %d\n", err);
		exit(1);
	}
}

#define EINTR WSAEINTR
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EINPROGRESS WSAEINPROGRESS
#define EISCONN WSAEISCONN
#else

#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>

#ifndef closesocket
# define closesocket close
#endif

static void lcs_startup()
{
}

#define LAST_ERROR() (errno)
#endif

#define CHECK_CLIENT(L, idx)\
	(*(sock_client_t **)luaL_checkudata(L, idx, LCS_CLIENT))

typedef struct sock_client_s {
	int fd;
	int connected;
	int connecting;
} sock_client_t;

static sock_client_t * lcsock_client_create()
{
	sock_client_t *p = malloc(sizeof(*p));
	if (p == NULL)
		goto nomem;
	p->fd = -1;
	p->connected = 0;
	p->connecting = 0;
	return p;
nomem:
	return NULL;
}

static int lua__lcs_new(lua_State *L)
{
	sock_client_t **p;
	sock_client_t *client;
	
	client = lcsock_client_create();
	if (client == NULL) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, "create client failed");
		return 2;
	}

	p = lua_newuserdata(L, sizeof(void *));
	*p = client;
	luaL_getmetatable(L, LCS_CLIENT);
	lua_setmetatable(L, -2);
	return 1;
}

static int lua__lcs_open(lua_State *L)
{
	int fd;
	sock_client_t * client = CHECK_CLIENT(L, 1);
	if(client->fd > 0)
	{
		return luaL_error(L, "already opened");
	}

	fd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd <= 0)
	{
		lua_pushboolean(L, 0);
		lua_pushnumber(L, LAST_ERROR());
		return 2;
	}

	client->fd = fd;
	lua_pushboolean(L, 1);
	return 1;
}

static int xx_setnoblock(int fd, int noblockFlag)
{
#ifdef OS_WINDOWS
	int flags = noblockFlag;
	return ioctlsocket(fd, FIONBIO, (u_long FAR*)&flags);
#else
	int flags = fcntl(fd, F_GETFL, 0);
	if(noblockFlag)
	{
		flags |= O_NONBLOCK;
	}
	else
	{
		flags &= ~O_NONBLOCK;
	}
	return fcntl(fd, F_SETFL, flags);
#endif
}

static int lua__lcs_setnonblock(lua_State *L)
{
	sock_client_t * client = CHECK_CLIENT(L, 1);
	xx_setnoblock(client->fd, 1);
	return 0;
}

static int lua__lcs_setsockopt(lua_State *L)
{
	sock_client_t * client = CHECK_CLIENT(L, 1);
	const char *optstr = luaL_checkstring(L, 2);
	lua_Number tv = luaL_checknumber(L, 3);
	int rc;
	int opt = 0;

#ifdef OS_WINDOWS
	int timeout = (int)tv;
#else
	struct timeval timeout ={
		(int)tv/1000,
		(((int)tv) % 1000) * 1e6
	};
#endif

	if (strcmp(optstr, "RCVTIMEO") == 0) {
		opt = SO_RCVTIMEO;
	} else if(strcmp(optstr, "SNDTIMEO") == 0) {
		opt = SO_SNDTIMEO;
	} else {
		return luaL_argerror(L, 2, "unknown opt");
	}

	rc = setsockopt(client->fd,
			SOL_SOCKET,
			opt,
			(const char*)&timeout,
			sizeof(timeout));
	lua_pushboolean(L, rc == 0);
	return 1;
}
static char s_lcsock_errorBuff[256];
static const char * tryConnect(int sockfd, struct addrinfo * res, int blockTimeMs)
{
	int ret;
	fd_set readFds;
	fd_set writeFds;
	struct timeval timeout;
	struct sockaddr_in peerAddr;
	socklen_t peerAddrLen;

	FD_ZERO(&readFds);
	FD_ZERO(&writeFds);

	FD_SET(sockfd, &writeFds);
	FD_SET(sockfd, &readFds);

	
	xx_setnoblock(sockfd, 1);


 	ret = connect(sockfd, res->ai_addr, res->ai_addrlen);
	if(ret == 0)
	{
		return 0;
	}

#ifdef OS_WINDOWS
	if( LAST_ERROR() != EWOULDBLOCK )
#else
	if( LAST_ERROR() != EINPROGRESS )
#endif
	{
		//连接没有立即返回，此时errno若不是EINPROGRESS，表明错误
		sprintf(s_lcsock_errorBuff, "connect failed! error=%d", LAST_ERROR());
	   	return s_lcsock_errorBuff;
	}

	timeout.tv_sec = blockTimeMs/1000;
	timeout.tv_usec = (blockTimeMs%1000)*1000;
	ret = select(sockfd+1, &readFds, &writeFds, NULL, &timeout);
	if(ret <= 0)
	{
		sprintf(s_lcsock_errorBuff, "select timeout or error! error=%d ret=%d", LAST_ERROR(), ret);
	   	return s_lcsock_errorBuff;
	}

	if(FD_ISSET(sockfd, &writeFds))
	{
		//可读可写有两种可能，一是连接错误，二是在连接后服务端已有数据传来
		if(FD_ISSET(sockfd, &readFds))
		{
			peerAddrLen = sizeof(peerAddr);
			if( 0 != getpeername(sockfd, (struct sockaddr*)&peerAddr, &peerAddrLen))
			{
				return "get socket option failed";
			}
		}

		//此时已排除所有错误可能，表明连接成功
		return 0;
	}

	return "connect failed: unknown";
}
/*
 *  start connect
 *	return succeed, connecting/errno
 */
static int lua__lcs_connect(lua_State *L)
{
	sock_client_t * client = CHECK_CLIENT(L, 1);
	const char *addrstr = luaL_checkstring(L, 2);
	const char *port =luaL_checkstring(L, 3);
	int blockTimeMs = luaL_optint(L, 4, 5000);

	struct addrinfo hints, *res, *res0;
	int error, s;
	const char *cause = NULL;
 
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo(addrstr, port, &hints, &res0);

	if (error != 0) {
		lua_pushboolean(L, 0);
		lua_pushnumber(L, error);
		return 2;
	}

	s = -1;
	for (res = res0; res; res = res->ai_next) {
		if(res->ai_family != AF_INET && res->ai_family != AF_INET6)
		{
			continue;
		}
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0) {
			cause = "socket";
			continue;
		}

		cause = tryConnect(s, res, blockTimeMs);
		if (cause) {
			closesocket(s);
			s = -1;
			continue;
		}
 
		break;  /* okay we got one */
	}

	freeaddrinfo(res0);
	if (s < 0) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, cause);
		return 2;
	}

	client->fd = s;
	client->connected = 1;
	client->connecting = 0;
	lua_pushboolean(L, 1);
	return 1;
}

/*
 *  check connecting is finished
 *	return noError, connected or error
 */
static int lua__lcs_checkconnected(lua_State *L)
{
	sock_client_t * client = CHECK_CLIENT(L, 1);
	int sockfd = client->fd;
	int n;
	fd_set rset, wset;
	struct timeval tval;

	LCS_DLOG("%s %d", __FUNCTION__, client->connected);

	if( 0 == client->connecting )
	{
		/* should never be called */
		lua_pushboolean(L, 0);
		lua_pushstring(L, "not_in_connecting");
		return 2;
	}

	if( 0 != client->connected )
	{
		client->connecting = 0;
		lua_pushboolean(L, 1);
		lua_pushboolean(L, 1);
		return 2;
	}

	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);

	FD_ZERO(&wset);
	FD_SET(sockfd, &wset);

	tval.tv_sec = 0;
	tval.tv_usec = 0;
	n = select(sockfd + 1, &rset, &wset, NULL, &tval );
	if ( n == 0 ) {
		/* time out */
		lua_pushboolean(L, 1);
		lua_pushboolean(L, 0);
		return 2;
	}

	if( n < 0)
	{
		/* error occor */
		lua_pushboolean(L, 0);
		lua_pushnumber(L, LAST_ERROR());
		client->connecting = 0;
		return 2;
	}

	if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset))
	{
		client->connecting = 0;
		client->connected = 1;

		lua_pushboolean(L, 1);
		lua_pushboolean(L, 1);
		return 2;
	}

	lua_pushboolean(L, 1);
	return 1;
}

static int lua__lcs_isconnected(lua_State *L)
{
	sock_client_t * client = CHECK_CLIENT(L, 1);
	LCS_DLOG("%s %d", __FUNCTION__, client->connected);
	lua_pushboolean(L, client->connected);
	return 1;
}

static int lua__lcs_isconnecting(lua_State *L)
{
	sock_client_t * client = CHECK_CLIENT(L, 1);
	LCS_DLOG("%s %d", __FUNCTION__, client->connecting);
	lua_pushboolean(L, client->connecting);
	return 1;
}

static int lua__lcs_disconnect(lua_State *L)
{
	sock_client_t * client = CHECK_CLIENT(L, 1);
	if(client->fd > 0)
	{
		closesocket(client->fd);
		client->fd = -1;
		client->connected = 0;
		client->connecting = 0;
	}
	return 0;
}
/*
	return buffer,wouldblock
*/
static int lua__lcs_recv(lua_State *L)
{
	char tmp[8192];
	int rsz = 0;
	int lastError;
	sock_client_t * client = CHECK_CLIENT(L, 1);
	size_t maxsz = luaL_optinteger(L, 2, sizeof(tmp));
	if (0 == client->connected) {
		return luaL_error(L, "not connected");
	}

	if (maxsz > sizeof(tmp)) {
		return luaL_error(L, "bad_maxsz");
	}

	rsz = recv(client->fd, tmp, (int)maxsz, 0);
	if (rsz > 0) 
	{
		lua_pushlstring(L, tmp, rsz);
		return 1;
	}
	else if (rsz == 0)
	{
		client->connected = 0;
		lua_pushnil(L);
		return 1;
	}
	else
	{
		lastError = LAST_ERROR();
	#ifdef OS_WINDOWS
		if (lastError == WSAEWOULDBLOCK || lastError == WSAEINTR )
	#else
		if (lastError == EWOULDBLOCK || lastError == EAGAIN || lastError == EINTR )
	#endif
		{
			lua_pushnil(L);
			lua_pushboolean(L, 1);
			return 2;
		}

		client->connected = 0;
		lua_pushnil(L);
		return 1;
	}
	return 0;
}

/*
	return sentLen,wouldblock
*/
static int lua__lcs_send(lua_State *L)
{
	size_t sz;
	int lastError;
	sock_client_t * client = CHECK_CLIENT(L, 1);
	const char *buf = luaL_checklstring(L, 2, &sz);

	int sentLen = send(client->fd, buf, (int)sz, 0);
	if (sentLen < 0)
	{
		lua_pushnil(L);
		lastError = LAST_ERROR();
	#ifdef OS_WINDOWS
		if (lastError == WSAEWOULDBLOCK || lastError == WSAEINTR )
	#else
		if (lastError == EWOULDBLOCK || lastError == EAGAIN || lastError == EINTR )
	#endif
		{
			lua_pushnumber(L, 1);
			return 2;
		}
		else
		{
			client->connected = 0;
			return 1;
		}
	}

	lua_pushnumber(L, sentLen);
	return 1;
}

static int lua__lcs_gc(lua_State *L)
{
	sock_client_t * client = CHECK_CLIENT(L, 1);
	if (client != NULL)
		free(client);
	LCS_DLOG("client gc");
	return 0;
}

static int opencls__client(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"open", lua__lcs_open},
		{"setnonblock", lua__lcs_setnonblock},
		{"connect", lua__lcs_connect},
		{"checkconnected", lua__lcs_checkconnected},
		{"recv", lua__lcs_recv},
		{"send", lua__lcs_send},
		{"setsockopt", lua__lcs_setsockopt},
		{"isconnected", lua__lcs_isconnected},
		{"isconnecting", lua__lcs_isconnecting},
		{"disconnect", lua__lcs_disconnect},
		{NULL, NULL},
	};
	luaL_newmetatable(L, LCS_CLIENT);
	lua_newtable(L);
	luaL_register(L, NULL, lmethods);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction (L, lua__lcs_gc);
	lua_setfield (L, -2, "__gc");
	return 1;
}

int luaopen_lcsock(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"new", lua__lcs_new},
		{NULL, NULL},
	};
	lcs_startup();
	opencls__client(L);
	luaL_newlib(L, lfuncs);
	return 1;
}
