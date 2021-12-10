#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef SLUA_3RD_LOADED
#include <lua.h>
#include <lauxlib.h>
#include "skynet_malloc.h"
#endif

#include "rc4.h"

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

#define RC4_CLS_NAME "cls{rc4state}"

#define CHECK_RC4OBJ(L, n) luaL_checkudata(L, n, RC4_CLS_NAME)

static int lua__rc4_new(lua_State *L)
{
	size_t sz;
	const char *key = luaL_checklstring(L, 1, &sz);
	rc4_state_t *s = lua_newuserdata(L, sizeof(*s));
	rc4_init(s, (uint8_t *)key, (int)sz);
	luaL_getmetatable(L, RC4_CLS_NAME);
	lua_setmetatable(L, -2);
	return 1;
}

static int lua__rc4_gc(lua_State *L)
{
	return 0;
}

static int lua__rc4_reset(lua_State *L)
{
	size_t sz;
	rc4_state_t *s = CHECK_RC4OBJ(L, 1);
	const char *key = luaL_checklstring(L, 2, &sz);
	rc4_init(s, (uint8_t *)key, (int)sz);
	return 0;
}

static int lua__rc4_update(lua_State *L)
{
	size_t sz;
	rc4_state_t *s = CHECK_RC4OBJ(L, 1);
	const char *data = luaL_checklstring(L, 2, &sz);
	char *out = malloc(sz);
	if (out == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, "nomem");
		return 2;
	}
	rc4_crypt(s, (uint8_t *)data, (uint8_t *)out, (int)sz);
	lua_pushlstring(L, out, sz);
	free(out);
	return 1;
}

const int MOD_ADLER = 65521;
static uint32_t adler32(unsigned char *data, size_t len)
{
	size_t i = 0;
	uint32_t a = 1, b = 0;
	
	
	/* Process each byte of the data in order */
	for (i = 0; i < len; ++i)
	{
		a = (a + data[i]) % MOD_ADLER;
		b = (b + a) % MOD_ADLER;
	}
	return (b << 16) | a;
}

/*客户端调用：
	@param str
	@param index

	@return str
*/
static int lua__rc4_encode(lua_State *L)
{
	size_t sz;
	char *out;
	uint32_t sum, sum_high, sum_low;
	rc4_state_t *s = CHECK_RC4OBJ(L, 1);
	const char *data = luaL_checklstring(L, 2, &sz);
	uint16_t index = luaL_checkinteger(L, 3);

	/* length(2) + msg(N) + sum(4) + index(2) */
	char *ptr = malloc(sz + 8);
	if (ptr == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, "nomem");
		return 2;
	}
	out = ptr + 2;

	rc4_crypt(s, (uint8_t *)data, (uint8_t *)out, (int)sz);
	sum = adler32((unsigned char *)out, sz);
	sum_high = sum / 65536;
	sum_low = sum % 65536;
	//printf("encode %u %u\n", sum, index);

	ptr[0] = (sz + 6) / 256;
	ptr[1] = (sz + 6) % 256;

	out[sz + 0] = sum_high / 256;
	out[sz + 1] = sum_high % 256;
	out[sz + 2] = sum_low / 256;
	out[sz + 3] = sum_low % 256;
	out[sz + 4] = index / 256;
	out[sz + 5] = index % 256;

	lua_pushlstring(L, ptr, sz + 8);
	free(ptr);
	return 1;
}
/*服务器调用：
	@param dat
	@param size

	@return strMsg, realSum, oriSum, index
*/
#ifndef SLUA_3RD_LOADED
static int lua__rc4_decode(lua_State *L)
{
	uint32_t sum, oriSum;
	uint32_t idx;
	rc4_state_t *s = CHECK_RC4OBJ(L, 1);
	unsigned char *data = (unsigned char *)lua_touserdata(L, 2);
	int size = luaL_checkinteger(L, 3);
	int check_index = luaL_optinteger(L, 4, -1);
	
	if(!data)
	{
		return luaL_argerror(L, 2, "null str");
	}
	if(size <= 6 || size > 65535)
	{
		skynet_free(data);
		
		lua_pushnil(L);
		lua_pushstring(L, "bad size");
		return 2;
	}

	idx = ((uint32_t)data[size - 2]) << 8 | ((uint32_t)data[size - 1]);
	if(check_index >= 0 && idx != check_index)
	{
		skynet_free(data);

		lua_pushnil(L);
		lua_pushstring(L, "bad index");
		return 2;
	}

	sum = adler32((unsigned char *)data, size - 6);
	oriSum = ((uint32_t)data[size - 6]) << 24 | ((uint32_t)data[size - 5]) << 16 | ((uint32_t)data[size - 4]) << 8 | ((uint32_t)data[size - 3]);
	if(sum != oriSum)
	{
		skynet_free(data);

		lua_pushnil(L);
		lua_pushstring(L, "sum mismatch");
		return 2;
	}
	char *out = malloc(size - 6);
	if (out == NULL) {
		skynet_free(data);

		lua_pushnil(L);
		lua_pushstring(L, "nomem");
		return 2;
	}
	rc4_crypt(s, (uint8_t *)data, (uint8_t *)out, (int)size - 6);
	
	lua_pushlstring(L, out, size - 6);
	lua_pushinteger(L, idx);
	free(out);
	skynet_free(data);
	return 2;
}
#endif

static int lua__rc4_xor(lua_State *L)
{
	size_t sz;
	size_t i = 0;
	uint32_t * pA, *pB;
	const char *data = luaL_checklstring(L, 1, &sz);
	uint32_t key = (uint32_t)luaL_checkinteger(L, 2);
	uint8_t *out = (uint8_t *)malloc(sz);
	if (out == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, "nomem");
		return 2;
	}

	for(i = 0;i < sz / 4;++i)
	{
		pA = (uint32_t *)(out + i * 4);
		pB = (uint32_t *)(data + i * 4);
		*pA = key ^ (*pB);
	}	
	lua_pushlstring(L, (const char*) out, sz);
	free(out);
	return 1;
}


static int lua__rc4_xor_pack(lua_State *L)
{
	size_t sz;
	size_t i = 0;
	const char *data = luaL_checklstring(L, 1, &sz);
	uint8_t key = (uint8_t)luaL_checkinteger(L, 2);
	uint8_t *out, *ptr;

	if(sz > 65530)
	{
		lua_pushnil(L);
		lua_pushstring(L, "string longer than 64K");
		return 2;
	}
	ptr = (uint8_t *)malloc(sz + 2);
	if (ptr == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, "nomem");
		return 2;
	}

	ptr[0] = sz / 256;
	ptr[1] = sz % 256;
	out = ptr + 2;
	for(i = 0;i < sz  ;++i)
	{
		out[i] = data[i] ^ key;
	}

	lua_pushlstring(L, (const char*) ptr, sz+2);
	free(ptr);
	return 1;
}

static int lua__rc4_xor_unpack(lua_State *L)
{
	size_t sz, packLen;
	size_t i = 0;
	uint8_t *data = (uint8_t*)luaL_checklstring(L, 1, &sz);
	uint32_t key = (uint32_t)luaL_checkinteger(L, 2);
	int startIndex = (int)luaL_optinteger(L, 3, 0);
	uint8_t *out;

	if(startIndex < 0 || startIndex + 2 >= sz)
	{
		return luaL_argerror(L, 3, "bad start index");
	}
	data += startIndex;
	if( sz < 2 )
	{
		lua_pushnil(L);
		return 1;
	}
	packLen = data[0] * 256 + data[1];
	if(packLen == 0)
	{
		lua_pushlstring(L, "", 0);
		return 1;
	}

	if(packLen + 2 + startIndex > sz)
	{
		lua_pushnil(L);
		return 1;
	}

	out = (uint8_t *)malloc(packLen);
	if (out == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, "nomem");
		return 2;
	}
	data += 2;
	
	for(i = 0;i < packLen;++i)
	{
		out[i] = data[i] ^ key;
	}
	lua_pushlstring(L, (const char*) out, packLen);
	free(out);
	return 1;
}


static int lua__rc4_adler32(lua_State *L)
{
	size_t len;
	unsigned char *data = (unsigned char *)luaL_checklstring(L, 1, &len);
	lua_pushinteger(L, adler32(data, len));
	return 1;
}

static int lua__rc4_adler32_check(lua_State *L)
{
	size_t len;
	uint32_t sum, oriSum, idx;
	unsigned char *data = (unsigned char *)luaL_checklstring(L, 1, &len);
	if(len <=6)
	{
		lua_pushboolean(L, 0);
		return 1;
	}
	sum = adler32(data, len - 6);
	oriSum = ((uint32_t)data[len - 6]) << 24 | ((uint32_t)data[len - 5]) << 16 | ((uint32_t)data[len - 4]) << 8 | ((uint32_t)data[len - 3]);
	idx = ( (uint32_t)data[len - 2] ) << 8 | ( (uint32_t)data[len - 1] );
	lua_pushboolean(L, sum == oriSum);
	lua_pushinteger(L, idx);
	return 2;
}
#ifndef SLUA_3RD_LOADED
static int lua__rc4_tostring(lua_State *L)
{
	uint32_t sum, oriSum;
	uint32_t idx;
	unsigned char *data = (unsigned char *)lua_touserdata(L, 1);
	int size = luaL_checkinteger(L, 2);
	
	if(!data)
	{
		return luaL_argerror(L, 1, "null str");
	}
	if(size <= 6 || size > 65535)
	{
		return luaL_argerror(L, 2, "bad size");
	}
	sum = adler32(data, size - 6);
	oriSum = ((uint32_t)data[size - 6]) << 24 | ((uint32_t)data[size - 5]) << 16 | ((uint32_t)data[size - 4]) << 8 | ((uint32_t)data[size - 3]);
	idx = ((uint32_t)data[size - 2]) << 8 | ((uint32_t)data[size - 1]);

	lua_pushlstring(L, (const char *)data, size - 6);
	skynet_free(data);

	lua_pushinteger(L, sum);
	lua_pushinteger(L, oriSum);
	lua_pushinteger(L, idx);
	return 4;
}
#endif

static int opencls__rc4(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"__gc", lua__rc4_gc },
		{"reset", lua__rc4_reset},
		{"update", lua__rc4_update},
		{"encode", lua__rc4_encode},
#ifndef SLUA_3RD_LOADED
		{"decode", lua__rc4_decode},
#endif
		{NULL, NULL},
	};
	luaL_newmetatable(L, RC4_CLS_NAME);
	luaL_newlib(L, lmethods);
	lua_setfield(L, -2, "__index");
	return 1;
}

int luaopen_lrc4(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"new", 			lua__rc4_new},
		{"xor", 			lua__rc4_xor},
		{"adler32", 		lua__rc4_adler32},
		{"adler32_check", 	lua__rc4_adler32_check},
		{"xor_pack", 		lua__rc4_xor_pack},
		{"xor_unpack", 		lua__rc4_xor_unpack},
#ifndef SLUA_3RD_LOADED
		{"tostring",		lua__rc4_tostring},
#endif
		{NULL, NULL},
	};
	opencls__rc4(L);
	luaL_newlib(L, lfuncs);
	return 1;
}
