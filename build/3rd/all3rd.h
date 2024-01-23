#define COMPILE_3RD

#ifdef COMPILE_3RD

#ifndef SLUA_3RD_LOADED
#define SLUA_3RD_LOADED

#if (defined(WIN32) || defined(_WIN32))
# pragma comment (lib,"ws2_32.lib")
#include <winsock2.h>
#include <ws2tcpip.h>
#endif


# include "lcsock.c"
# include "ldump.c"
# include "lmsgpack.c"
# include "lcjson/strbuf.c"
# include "lcjson/lcjson.c"
#include "rc4/lrc4.c"
#include "rc4/rc4.c"
# include "lmisc.c"
# include "luniq/handlemap.c"
# include "luniq/luniq.c"
# include "proc/proc.h"
# include "proc/proc.c"
# include "proc/lproc.c"
#include "llzf/lzf_c.c"
#include "llzf/lzf_d.c"
#include "llzf/llzf.c"
# include "lfs.c"

# include "lsinfo.c"
#include "skiplist/skiplist.c"
#include "skiplist/lskiplist.c"


#include "sproto/sproto.c"
#include "sproto/lsproto.c"

#include "lpeg/lpcap.c"
#include "lpeg/lpcode.c"
#include "lpeg/lpprint.c"
#include "lpeg/lptree.c"
#include "lpeg/lpvm.c"
# include "ltrace.c"
# include "lcoredump.c"
#include "llz4/lua_lz4.c"
#include "lheap.c"

// #include "lsqlite/lsqlite3.c"
// #include "lsqlite/sqlite3.h"
// #include "lsqlite/sqlite3.c"



#define EXTEND_LUA_LIB_MAP(XX)            \
	XX(lcsock, luaopen_lcsock)        \
	XX(ldump, luaopen_ldump)          \
	XX(msgpack, luaopen_msgpack)      \
	XX(cjson, luaopen_cjson)          \
	XX(lrc4, luaopen_lrc4)            \
	XX(misc, luaopen_misc)            \
	XX(luniq, luaopen_luniq)          \
	XX(proc, luaopen_proc)            \
	XX(lzf, luaopen_lzf)              \
	XX(lfs, luaopen_lfs)              \
	XX(lsinfo, luaopen_lsinfo)        \
	XX(lskiplist, luaopen_lskiplist)  \
	XX(lsproto, luaopen_lsproto)      \
	XX(lpeg, luaopen_lpeg)            \
	XX(ltrace, luaopen_ltrace)        \
	XX(lcoredump, luaopen_lcoredump)  \
	XX(llz4, luaopen_llz4)\
	XX(lheap, luaopen_lheap)\
	//XX(lsqlite3, luaopen_lsqlite3)    \


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
