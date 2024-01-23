
#define LUA_LIB

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include <string.h>
#include <stdint.h>


#if LUA_VERSION_NUM < 503
#define lua_absindex(L, index) ((index > 0 || index <= LUA_REGISTRYINDEX) ? index : lua_gettop(L) + index + 1)
#endif

#define lp_equal(L,idx1,idx2)  lua_compare(L,(idx1),(idx2),LUA_OPEQ)

#include "all3rd.h"

