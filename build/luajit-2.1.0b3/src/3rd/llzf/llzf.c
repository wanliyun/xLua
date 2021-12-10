#include <stdlib.h>
#include <errno.h>

/**
 * #include <lauxlib.h>
 * #include <lua.h>
 */

#include "lzf.h"
#include "compat.h"

#if LUA_VERSION_NUM < 502
# ifndef luaL_newlib
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
# endif
# ifndef lua_setuservalue
#  define lua_setuservalue(L, n) lua_setfenv(L, n)
# endif
# ifndef lua_getuservalue
#  define lua_getuservalue(L, n) lua_getfenv(L, n)
# endif
#endif

#define LIBNAME "lzf"
#define RETRY_MAX_CNT 10

static int lua__compress(lua_State *L)
{
	size_t size;
	unsigned int out_len;
	void *out_data;
	unsigned int clen;
	const char *in_data = luaL_checklstring(L, 1, &size);
	out_len = size * 1.04f + 8;
	out_data = malloc(sizeof(char) * out_len);
	if (out_data == NULL) {
		lua_pushnil(L);
		lua_pushfstring(L, "ENOMEM in %s", __FUNCTION__);
		return 0;
	}
	clen = lzf_compress(in_data, (unsigned int)size,
		     out_data, out_len);
	if (clen == 0) {
		free(out_data);
		lua_pushnil(L);
		lua_pushfstring(L, "compress failed in %s", __FUNCTION__);
		return 2;
	}
	lua_pushlstring(L, (const char *)out_data, clen);
	free(out_data);
	return 1;
}

static int lua__decompress(lua_State *L)
{
	const float ratio = 2.0f;
	size_t size;
	unsigned int out_len;
	void *out_data;
	unsigned int clen;
	const char *in_data = luaL_checklstring(L, 1, &size);
	int retrycnt = 0;
	out_len = size * 5.0f;
	do {
		out_len *= ratio;
		out_data = malloc(sizeof(char) * out_len);
		if (out_data == NULL) {
			lua_pushnil(L);
			lua_pushfstring(L, "ENOMEM in %s", __FUNCTION__);
			return 0;
		}
		clen = lzf_decompress(in_data, (unsigned int)size,
			     out_data, out_len);
		retrycnt++;
		if (clen == 0) {
			free(out_data);
			if (errno == E2BIG) {
				continue;
			}
			break;
		}
		lua_pushlstring(L, (const char *)out_data, clen);
		free(out_data);
		return 1;
	} while (1);
	lua_pushnil(L);
	lua_pushfstring(L, "ENOMEM in %s", __FUNCTION__);
	return 2;
}
/****************************************************************************/
struct slzf {
	char * buffer;
	size_t bufferSize;
};

#define CHECK_LZF(L, idx)	(struct slzf*)luaL_checkudata(L, idx, LIBNAME)

static int llzf_create(lua_State* L)
{
	int bufferSize = 0;
	char * buffer = 0;
	struct slzf * ret  = 0;
	bufferSize = luaL_optinteger(L, 1, 256 * 1024);
	if(bufferSize <= 512){
		return luaL_error(L, "Failed to alloc mem size=%d", bufferSize);
	}
	
	buffer = (char *)malloc(bufferSize * sizeof(char));
	if(0 == buffer){
		return luaL_error(L, "Failed to alloc mem size=%d", bufferSize);
	}
	
	ret = (struct slzf*)lua_newuserdata(L, sizeof(struct slzf));
	ret->buffer = buffer;
	ret->bufferSize = bufferSize;
	
	luaL_getmetatable(L, LIBNAME);
	lua_setmetatable(L, -2);
	
	return 1;
}

static int llzf_destroy(lua_State* L)
{
	struct slzf * lzf = CHECK_LZF(L, 1);
	if (lzf->buffer) {
		free(lzf->buffer);
		lzf->buffer = 0;
	}

	return 0;
}

int llzf_compress(lua_State *L)
{
	struct slzf * lzf = CHECK_LZF(L, 1);
	size_t in_len;
	const char * in = luaL_checklstring(L, 2, &in_len);

	unsigned int clen = lzf_compress(in, (unsigned int)in_len, lzf->buffer, lzf->bufferSize);
	
	if (clen == 0)
	{
		lua_pushnil(L);
		lua_pushstring(L, "lzf was not able of encoding the data");
		return 2;
	}
	lua_pushlstring(L, (const char *)lzf->buffer, clen);
	
	return 1;
}

int llzf_decompress(lua_State *L)
{
	size_t in_len;
	struct slzf * lzf = CHECK_LZF(L, 1);
	const char * in = luaL_checklstring(L, 2, &in_len);
	
	unsigned int dlen = lzf_decompress(in, (unsigned int)in_len, lzf->buffer, lzf->bufferSize);
	
	if (dlen == 0)
	{
		lua_pushnil(L);
		lua_pushstring(L, "LZF wasn't able to decode the data");
		return 2;
	}
	lua_pushlstring(L, (const char *)lzf->buffer, dlen);
	return 1;
}

static void opencls_lzf(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"compress", llzf_compress},
		{"decompress", llzf_decompress},
		{NULL, NULL},
	};

	luaL_newmetatable(L, LIBNAME);
	luaL_newlib(L, lmethods);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction (L, llzf_destroy);
	lua_setfield (L, -2, "__gc");
}

int luaopen_lzf(lua_State *L)
{
	luaL_Reg lfuncs[] = {
		{"create", llzf_create},
		{"compress", lua__compress},
		{"decompress", lua__decompress},
		{NULL, NULL}
	};
	
	opencls_lzf(L);
	luaL_newlib(L, lfuncs);
	return 1;
}

#undef LIBNAME
