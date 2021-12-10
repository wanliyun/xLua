#include <stdio.h>
#include <stdlib.h>
/** 
 * #include "lua.h"
 * #include "lauxlib.h"
 */

#include "proc.h"

#if LUA_VERSION_NUM < 502 && (!defined(luaL_newlib))
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif


static int lua__css(lua_State *L)
{
	size_t ret = getProcCurrentRSS();
	lua_pushnumber(L, ret);
	return 1;
}

static int lua__pss(lua_State *L)
{
	size_t ret = getProcPeakRSS();
	lua_pushnumber(L, ret);
	return 1;
}

static int lua__phmem(lua_State *L)
{
	size_t ret = getPhyMemorySize();
	lua_pushnumber(L, ret);
	return 1;
}

static int lua__cputime(lua_State *L)
{
	double ret = getCPUTime();
	lua_pushnumber(L, ret);
	return 1;
}

static int lua__cpuusage(lua_State *L)
{
	double ret = getCPUUsage();
	lua_pushnumber(L, ret);
	return 1;
}

int luaopen_proc(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"css", lua__css},
		{"pss", lua__pss},
		{"phmem", lua__phmem},
		{"cputime", lua__cputime},
		{"cpuusage", lua__cpuusage},
		{NULL, NULL},
	};
	luaL_newlib(L, lfuncs);
	return 1;
}
