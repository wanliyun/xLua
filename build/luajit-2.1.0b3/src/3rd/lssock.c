#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>



#if ((LUA_VERSION_NUM < 502) && (!defined(luaL_newlib)))
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

#define PEER "lssock{peer}"
#define SERVER "lssock{server}"

// #define ENABLE_LSSOCK_DEBUG
#ifdef ENABLE_LSSOCK_DEBUG
# define DLOG(fmt, ...) fprintf(stderr, "<lssock>" fmt "\n", ##__VA_ARGS__)
#else
# define DLOG(...)
#endif

#define CHECK_PEER(L, idx)\
	(*(peer_t **) luaL_checkudata(L, idx, PEER))

#define CHECK_SERVER(L, idx)\
	(*(server_t **) luaL_checkudata(L, idx, SERVER))


#define LUA_BIND_META(L, type_t, ptr, mname) do {                   \
	type_t **my__p = lua_newuserdata(L, sizeof(void *));        \
	*my__p = ptr;                                               \
	luaL_getmetatable(L, mname);                                \
	lua_setmetatable(L, -2);                                    \
} while(0)

#if defined(WIN32)  || defined(_WIN32)

typedef long ssize_t;
# define EINTR WSAEINTR
# define EWOULDBLOCK WSAEWOULDBLOCK
static void startup()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		printf("WSAStartup failed with error: %d\n", err);
		exit(1);
	}
}
#else
# define closesocket(fd) close(fd)
# include <sys/select.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <errno.h>
static void startup() {}
#endif

typedef struct peer_s {
	int sock;
} peer_t;

typedef struct server_s {
	int sock;
} server_t;

static int addr_parse(const char * addrstr,
		      int port,
		      struct sockaddr_in * addr)
{
	int nIP = 0;
	if (!addrstr || *addrstr == '\0' 
	    || strcmp(addrstr, "0") == 0
	    || strcmp(addrstr, "0.0.0.0") == 0
	    || strcmp(addrstr, "*") == 0
	    || strcmp(addrstr, "any") == 0) {
		nIP = htonl(INADDR_ANY);
	} else if (strcmp(addrstr, "localhost") == 0) {
		char tmp[] = "127.0.0.1";
		nIP = inet_addr((const char *)tmp);
	} else {
		nIP = inet_addr(addrstr);
	}
	addr->sin_addr.s_addr = nIP;
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	return 0;
}

peer_t * peer_create()
{
	peer_t * p = malloc(sizeof(*p));
	if (p == NULL)
		goto nomem;
	p->sock = -1;
	return p;
nomem:
	return NULL;
}

server_t * server_create()
{
	server_t * p = malloc(sizeof(*p));
	if (p == NULL)
		goto nomem;
	p->sock = -1;
	return p;
nomem:
	return NULL;
}

static int fdcanread(int fd)
{
	struct timeval tv = { 0, 0 };
	fd_set rfds;
	int r = 0;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	r = select(fd + 1, &rfds, NULL, NULL, &tv);
	return r == 1;
}

static int lua__sleep(lua_State *L)
{
	int ms = (int)luaL_optinteger(L, 1, 0);
#ifdef WIN32
	Sleep(ms);
#else
	usleep((useconds_t)ms * 1000);
#endif
	lua_pushboolean(L, 1);
	return 1;
}

static int lua__isconnect(lua_State *L)
{
	peer_t *peer = CHECK_PEER(L, 1);
	if (peer == NULL || peer->sock <= 0)
		lua_pushboolean(L, 0);
	else
		lua_pushboolean(L, 1);
	return 1;
}

static int lua__read(lua_State *L)
{
	char tmp[8192];
	char *buf = (char *)&tmp;
	ssize_t rsz = 0;
	peer_t *peer = CHECK_PEER(L, 1);
	size_t sz = luaL_optlong(L, 2, sizeof(tmp));
	if (peer->sock <= 0) {
		return luaL_error(L, "not connected");
	}
	if (!fdcanread(peer->sock)) {
		DLOG("read no data");
		lua_pushboolean(L, 0);
		lua_pushstring(L, "no data");
		return 2;
	}
	if (sz > sizeof(tmp)) {
		buf = malloc(sz);
		if (buf == NULL) {
			return luaL_error(L, "nomem while read");
		}
	}
	
	rsz = recv(peer->sock, buf, sz, 0);
	if (rsz > 0) {
		lua_pushlstring(L, buf, rsz);
	} else if (rsz < 0) {
		DLOG("read again");
	} else {
		DLOG("read is closed");
		closesocket(peer->sock);
		peer->sock = -1;
	}
	if (buf != (char *)&tmp) {
		free(buf);
	}
	return rsz > 0 ? 1 : 0;
}

static int lua__write(lua_State *L)
{
	size_t sz;
	size_t p = 0;
	peer_t *peer = CHECK_PEER(L, 1);
	const char *buf = luaL_checklstring(L, 2, &sz);
	if (peer->sock <= 0) {
		return luaL_error(L, "not connected");
	}
	for (;;) {
		ssize_t wt = send(peer->sock, buf + p, sz - p, 0);
		if (wt < 0) {
			switch (errno) {
			case EWOULDBLOCK:
			case EINTR:
				continue;
			default:
				closesocket(peer->sock);
				peer->sock = -1;
				lua_pushboolean(L, 0);
				return 1;
			}
		}
		if (wt == sz - p)
			break;
		p += wt;
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int lua__close(lua_State *L)
{
	peer_t *peer = CHECK_PEER(L, 1);
	if (peer == NULL || peer->sock <= 0) {
		return 0;
	}
	DLOG("peer close, sock=%d", peer->sock);
	closesocket(peer->sock);
	peer->sock = -1;
	return 0;
}

static int lua__getpeername(lua_State *L)
{
	struct sockaddr_in sa;
	socklen_t addrlen = sizeof(sa);
	const char *host = NULL;
	peer_t *peer = CHECK_PEER(L, 1);
	if (peer == NULL || peer->sock < 0 ) {
		return luaL_error(L, "peer required or peer is closed");
	}
	if (getpeername(peer->sock, (struct sockaddr *)&sa, &addrlen)) {
		lua_pushnil(L);
		lua_pushstring(L, "failed to getpeername");
		return 2;
	}
	host = inet_ntoa(sa.sin_addr);
	lua_pushstring(L, host);
	lua_pushinteger(L, ntohs(sa.sin_port));
	return 2;
}

static int lua__peer_gc(lua_State *L)
{
	peer_t *peer = CHECK_PEER(L, 1);
	if (peer == NULL)
		return 0;
	if (peer->sock > 0) {
		closesocket(peer->sock);
		peer->sock = -1;
	}
	free(peer);
	return 0;
}

static int opencls__peer(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"isconnected", lua__isconnect},
		{"read", lua__read},
		{"write", lua__write},
		{"close", lua__close},
		{"getpeername", lua__getpeername},
		{NULL, NULL},
	};
	luaL_newmetatable(L, PEER);
	lua_newtable(L);
	luaL_register(L, NULL, lmethods);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction (L, lua__peer_gc);
	lua_setfield (L, -2, "__gc");
	return 1;
}

static int lua__bind(lua_State *L)
{
	struct sockaddr_in addr;
	server_t * s = CHECK_SERVER(L, 1);
	const char *addrstr = luaL_checkstring(L, 2);
	int port = (int)luaL_checkinteger(L, 3);
	if (port <= 0 || port > 65535) {
		return luaL_error(L, "port range [0, 65535]");
	}
	addr_parse(addrstr, port, &addr);
	if (bind(s->sock, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
		closesocket(s->sock);
		lua_pushnil(L);
		lua_pushfstring(L, "bind %s:%d failed", addrstr, port);
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int lua__listen(lua_State *L)
{
	server_t * s = CHECK_SERVER(L, 1);
	int backlog = (int)luaL_optinteger(L, 2, 32);
	if (listen(s->sock, backlog) < 0) {
		lua_pushnil(L);
#ifndef WIN32
		lua_pushfstring(L, "listen failed : %s", strerror(errno));
#else
		lua_pushstring(L, "listen failed");
#endif
		return 2;
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int lua__accept(lua_State *L)
{
	int fd;
	peer_t *peer;
	server_t * s = CHECK_SERVER(L, 1);
	if (s->sock < 0) {
		return luaL_error(L, "bind or listen first!");
	}
	if (!fdcanread(s->sock)) {
		return 0;
	}
	fd = accept(s->sock, NULL, NULL);
	if (fd < 0) {
		lua_pushnil(L);
		lua_pushstring(L, "accept error");
		return 0;
	}
	peer = peer_create();
	if (peer == NULL) {
		closesocket(fd);
		lua_pushnil(L);
		lua_pushstring(L, "nomem");
		return 2;
	}
	peer->sock = fd;
	LUA_BIND_META(L, peer_t, peer, PEER);

	DLOG("new peer connected sock=%d", fd);
	return 1;
}

static int lua__shutdown(lua_State *L)
{
	server_t * s = CHECK_SERVER(L, 1);
	if (s->sock > 0) {
		DLOG("server shutdown sock=%d", s->sock);
		closesocket(s->sock);
		s->sock = -1;
	}
	return 0;
}

static int lua__server_gc(lua_State *L)
{
	server_t * s = CHECK_SERVER(L, 1);
	DLOG("server gc sock=%d", s->sock);
	if (s->sock > 0) {
		closesocket(s->sock);
		s->sock = -1;
	}
	free(s);
	return 0;
}

static int opencls__server(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"bind", lua__bind},
		{"listen", lua__listen},
		{"accept", lua__accept},
		{"shutdown", lua__shutdown},
		{NULL, NULL},
	};
	luaL_newmetatable(L, SERVER);
	lua_newtable(L);
	luaL_register(L, NULL, lmethods);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction (L, lua__server_gc);
	lua_setfield (L, -2, "__gc");
	return 1;
}

static int lua__newserver(lua_State *L)
{
	server_t * p;
	int reuse = 1;
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		lua_pushnil(L);
		lua_pushstring(L, "create socket failed!");
		return 2;
	}
	p = server_create();
	if (p == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, "create socket failed!");
		return 2;
	}
	LUA_BIND_META(L, server_t, p, SERVER);
	p->sock = sock;
        setsockopt(p->sock,
		   SOL_SOCKET,
		   SO_REUSEADDR,
		   (void *)&reuse,
		   sizeof(int));
	return 1;
}

static int lua__select_peer(lua_State *L)
{
	fd_set rfds;
	int i;
	int j;
	int r = 0;
	int max = 0;
	int array[1024] = {0};
	int argc = lua_gettop(L);
	double interval = luaL_optnumber(L, 1, 0.0);
	struct timeval tv = { (long)(interval/1e3), ((int64_t)interval) % 1000 * 1000 };

	if (argc != 2) {
		return luaL_error(L, "2 args required in select");
	}

	memset(array, 0, sizeof(array));

	FD_ZERO(&rfds);

	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		int idx = (int)lua_tointeger(L, -2);
		peer_t *peer = CHECK_PEER(L, -1);
		if (peer->sock > 0) {
			if (peer->sock > max)
				max = peer->sock;
			FD_SET(peer->sock, &rfds);
			array[peer->sock] = idx;
		}
		lua_pop(L, 1);
	}

	r = select(max + 1, &rfds, NULL, NULL, &tv);
	lua_newtable(L);
	if (r <= 0) {
		return 1;
	}
	j = 0;
	for (i = 0; i <= max; i++) {
		if(FD_ISSET(i, &rfds)) {
			int idx = array[i];
			j++;
			lua_rawgeti(L, -2, idx);
			lua_rawseti(L, -2, j);
		}
	}
	return 1;
}

static int luac__is_classtype(lua_State *L, int idx, const char *classType)
{
	int ret = 0;
	int top = lua_gettop(L);
	int mt = lua_getmetatable(L, idx);
	if (mt == 0) {
		goto finished;
	}
	luaL_getmetatable(L, classType);
	if (lua_equal(L, -1, -2)) {
		ret = 1;
	}
finished:
	lua_settop(L, top);
	return ret;
}

static int lua__is_server(lua_State *L)
{
	int ret = luac__is_classtype(L, 1, SERVER);
	lua_pushboolean(L, ret);
	return 1;
}

static int lua__is_peer(lua_State *L)
{
	int ret = luac__is_classtype(L, 1, PEER);
	lua_pushboolean(L, ret);
	return 1;
}

int luaopen_lssock(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"new", lua__newserver},
		{"sleep", lua__sleep},
		{"select", lua__select_peer},
		{"is_server", lua__is_server},
		{"is_peer", lua__is_peer},
		{NULL, NULL},
	};
	startup();
	opencls__peer(L);
	opencls__server(L);
	luaL_newlib(L, lfuncs);
	return 1;
}
