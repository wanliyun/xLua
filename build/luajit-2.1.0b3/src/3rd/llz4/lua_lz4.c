
/**************************************
*  Common LZ4 definition
**************************************/

#include "lz4/lz4hc.c"

#if LUA_VERSION_NUM >= 502
#define LUABUFF_NEW(lua_buff, c_buff, max_size) \
  luaL_Buffer lua_buff;                         \
  char *c_buff = luaL_buffinitsize(L, &lua_buff, max_size);
#define LUABUFF_FREE(c_buff)
#define LUABUFF_PUSH(lua_buff, c_buff, size)    \
  luaL_pushresultsize(&lua_buff, size);
#else
#define LUABUFF_NEW(lua_buff, c_buff, max_size) \
  char *c_buff = malloc(max_size);              \
  if (c_buff == NULL) return luaL_error(L, "out of memory");
#define LUABUFF_FREE(c_buff)                    \
  free(c_buff);
#define LUABUFF_PUSH(lua_buff, c_buff, size)    \
  lua_pushlstring(L, c_buff, size);             \
  free(c_buff);
#endif

/*****************************************************************************
* Block
****************************************************************************/
static int lz4_block_compress(lua_State *L)
{
	size_t in_len;
	const char *in = luaL_checklstring(L, 1, &in_len);
	int accelerate = (int)luaL_optinteger(L, 2, 0);
	int bound, r;

	if (in_len > LZ4_MAX_INPUT_SIZE)
	{
		return luaL_error(L, "input longer than %d", LZ4_MAX_INPUT_SIZE);
	}

	bound = LZ4_compressBound((int)in_len);
	{
		LUABUFF_NEW(b, out, bound)
		r = (int)LZ4_compress_fast(in, out, in_len, bound, accelerate);
		if (r == 0)
		{
			LUABUFF_FREE(out)
			return luaL_error(L, "compression failed");
		}
		LUABUFF_PUSH(b, out, r)
	}
	return 1;
}

static int lz4_block_compress_hc(lua_State *L)
{
	size_t in_len;
	const char *in = luaL_checklstring(L, 1, &in_len);
	int level = (int)luaL_optinteger(L, 2, 0);
	int bound, r;

	if (in_len > LZ4_MAX_INPUT_SIZE)
	{
		return luaL_error(L, "input longer than %d", LZ4_MAX_INPUT_SIZE);
	}

	bound = LZ4_compressBound((int)in_len);

	{
		LUABUFF_NEW(b, out, bound)
		r = LZ4_compress_HC(in, out, (int)in_len, bound, level);
		if (r == 0)
		{
			LUABUFF_FREE(out)
			return luaL_error(L, "compression failed");
		}
		LUABUFF_PUSH(b, out, r)
	}
	return 1;
}

static int lz4_block_decompress_safe(lua_State *L)
{
	size_t in_len;
	const char *in = luaL_checklstring(L, 1, &in_len);
	int out_len = (int)luaL_checkinteger(L, 2);
	int r;

	LUABUFF_NEW(b, out, out_len)
	r = LZ4_decompress_safe(in, out, (int)in_len, out_len);
	if (r < 0)
	{
		LUABUFF_FREE(out)
		return luaL_error(L, "corrupt input or need more output space");
	}
	LUABUFF_PUSH(b, out, r)
	return 1;
}

/*****************************************************************************
* Export
****************************************************************************/
static const luaL_Reg export_functions[] = {
	/* Frame */
	{ "block_compress",			lz4_block_compress },
	{ "block_compress_hc",		lz4_block_compress_hc },
	{ "block_decompress",		lz4_block_decompress_safe },
	{ NULL,						NULL },
};

LUALIB_API int luaopen_llz4(lua_State *L)
{
	int table_index;
	luaL_newlib(L, export_functions);

	table_index = lua_gettop(L);

	lua_pushfstring(L, "%d.%d.%d", LZ4_VERSION_MAJOR, LZ4_VERSION_MINOR, LZ4_VERSION_RELEASE);
	lua_setfield(L, table_index, "version");
	return 1;
}
