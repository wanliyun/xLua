#define COMPILE_3RD

#ifdef COMPILE_3RD

#ifndef SLUA_3RD_LOADED
#define SLUA_3RD_LOADED

#if (defined(WIN32) || defined(_WIN32))
# pragma comment (lib,"ws2_32.lib")
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

/*
# include "astar/AStar.h"
# include "astar/AStar.c"
# include "astar/lastar.c"
*/

# include "bresenham/bresenham.h"
# include "bresenham/bresenham.c"
# include "lssock.c"
# include "lcsock.c"
# include "aoi/aoi.h"
# include "aoi/aoi.c"
# include "aoi/laoi.c"
# include "bit.c"
# include "ldump.c"
# include "lcjson/strbuf.c"
# include "lcjson/lcjson.c"
# include "lmsgpack.c"
# include "kcp/ikcp.c"
# include "kcp/lkcp.c"
# include "lbitstring/bitstring.h"
# include "lbitstring/lbitstring.c"
/*# include "ludp.c" */
# include "lfs.c"
# include "lsinfo.c"

# include "proc/proc.h"
# include "proc/proc.c"
# include "proc/lproc.c"

# include "htimer/htimer.c"
# include "htimer/lhtimer.c"
# include "ltrace.c"
# include "lcoredump.c"



#include "luniq/handlemap.c"
#include "luniq/luniq.c"

#include "lsqlite/sqlite3.h"
#include "lsqlite/sqlite3.c"
#include "lsqlite/lsqlite3.c"

#include "llzf/lzf_c.c"
#include "llzf/lzf_d.c"
#include "llzf/llzf.c"

#include "rc4/lrc4.c"
#include "rc4/rc4.c"
#include "lmisc.c"

#include "skiplist/skiplist.c"
#include "skiplist/lskiplist.c"

#include "sproto/sproto.c"
#include "sproto/lsproto.c"

#include "lpeg/lpcap.c"
#include "lpeg/lpcode.c"
#include "lpeg/lpprint.c"
#include "lpeg/lptree.c"
#include "lpeg/lpvm.c"

#include "llz4/lua_lz4.c"

#include "lheap.c"

#define EXTEND_LUA_LIB_MAP(XX)            \
	XX(laoi, luaopen_laoi)            \
	XX(lssock, luaopen_lssock)        \
	XX(lcsock, luaopen_lcsock)        \
	XX(ldump, luaopen_ldump)          \
	XX(msgpack, luaopen_msgpack)      \
	XX(cjson, luaopen_cjson)          \
	XX(misc, luaopen_misc)            \
	XX(lbit, luaopen_lbit)            \
	XX(luniq, luaopen_luniq)          \
	XX(lkcp, luaopen_lkcp)            \
	XX(proc, luaopen_proc)            \
	XX(lhtimer, luaopen_lhtimer)      \
	XX(lsqlite3, luaopen_lsqlite3)    \
	XX(lzf, luaopen_lzf)              \
	XX(lfs, luaopen_lfs)              \
	XX(lrc4, luaopen_lrc4)            \
	XX(lsinfo, luaopen_lsinfo)        \
	XX(lskiplist, luaopen_lskiplist)  \
	XX(lsproto, luaopen_lsproto)      \
	XX(lpeg, luaopen_lpeg)            \
	XX(ltrace, luaopen_ltrace)        \
	XX(lcoredump, luaopen_lcoredump)  \
	XX(lbs, luaopen_lbs)\
	XX(llz4, luaopen_llz4)\
	XX(lheap, luaopen_lheap)



void luaopen_all3rd(lua_State *L)
{
	int top = lua_gettop(L);

#define XX(libname, openfunc) (openfunc(L), lua_setglobal(L, #libname));
	EXTEND_LUA_LIB_MAP(XX)
#undef XX

	lua_settop(L, top);
}
#endif // SLUA_3RD_LOADED

#endif // end of COMPILE_3RD
