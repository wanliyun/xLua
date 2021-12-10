#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


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

#define SINFO_NAME "cls{scene_info}"
#define CHECK_SINFO(L, n) ((sinfo_t *)luaL_checkudata(L, n, SINFO_NAME))

#define FACTOR 10

typedef union pos_info_s {
	struct {
		uint16_t mask;
		int16_t height;
	}s;
	uint32_t d;
} pos_info_t;

typedef struct {
	uint32_t width;
	uint32_t height;
	pos_info_t pos[1];
} sinfo_t;

static int lua__scene_new(lua_State *L)
{
	int width = luaL_checkinteger(L, 1);
	int height = luaL_checkinteger(L, 2);
	size_t sz = (width * height - 1) * sizeof(pos_info_t) + sizeof(sinfo_t);
	sinfo_t *s = (sinfo_t *)lua_newuserdata(L, sz);

	memset(s, 0, sz);
	s->width = width;
	s->height = height;

	luaL_getmetatable(L, SINFO_NAME);
	lua_setmetatable(L, -2);
	return 1;
}

static int lua__scene_load(lua_State *L)
{
	size_t sz;
	const char *data = luaL_checklstring(L, 1, &sz);

	void *p = lua_newuserdata(L, sz);
	memcpy(p, data, sz);

	luaL_getmetatable(L, SINFO_NAME);
	lua_setmetatable(L, -2);
	return 1;
}

static int lua__scene_size(lua_State *L)
{
	sinfo_t *s = CHECK_SINFO(L, 1);
	lua_pushinteger(L, s->width);
	lua_pushinteger(L, s->height);
	return 2;
}

static int lua__scene_set(lua_State *L)
{
	int idx;
	pos_info_t *pos;
	sinfo_t *s = CHECK_SINFO(L, 1);
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	float height_full = luaL_checknumber(L, 4) * FACTOR + 0.5;
	uint32_t mask = (uint32_t)luaL_checkinteger(L, 5);

	if (x < 0 || x >= s->width)
		return luaL_error(L, "x=%d error range [0, %d]", x, s->width - 1);
	if (y < 0 || y >= s->height)
		return luaL_error(L, "y=%d error range [0, %d]", y, s->height - 1);

	if (mask >= 2<<16)
		return luaL_error(L, "mask error range [0, %d]", 2<<16);

	if (height_full < -32767 || height_full >= 32768) {
		return luaL_error(L, "height error [%f, %f]", -32767.0 / FACTOR, 32768.0/FACTOR);
	}

	idx = y * s->width + x;
	pos = &s->pos[idx];
	pos->s.height = (int16_t)height_full;
	pos->s.mask = (uint16_t)mask & 0xffff;
	return 0;
}

static int lua__scene_get(lua_State *L)
{
	int idx;
	pos_info_t *pos;
	sinfo_t *s = CHECK_SINFO(L, 1);
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);

	if (x < 0 || x >= s->width) {
		lua_pushnil(L);
		lua_pushfstring(L, "x=%d error range[0, %d]", x, s->width - 1);
		return 2;
		// return luaL_error(L, "x=%d error range[0, %d]", x, s->width - 1);
	}
	if (y < 0 || y >= s->height) {
		lua_pushnil(L);
		lua_pushfstring(L, "y=%d error range[0, %d]", y, s->height - 1);
		return 2;
		// return luaL_error(L, "y=%d error range[0, %d]", y, s->height - 1);
	}

	idx = y * s->width + x;
	pos = &s->pos[idx];

	lua_pushnumber(L, (float)pos->s.height * 1.0 / FACTOR);
	lua_pushinteger(L, pos->s.mask);
	return 2;
}

static int lua__scene_memsize(lua_State *L)
{
	sinfo_t *s = CHECK_SINFO(L, 1);
	size_t sz = (s->width * s->height - 1) * sizeof(pos_info_t) + sizeof(sinfo_t);
	lua_pushnumber(L, sz);
	return 1;
}

static int lua__scene_tostring(lua_State *L)
{
	sinfo_t *s = CHECK_SINFO(L, 1);
	size_t sz = (s->width * s->height - 1) * sizeof(pos_info_t) + sizeof(sinfo_t);
	lua_pushlstring(L, (const char *)s, sz);
	return 1;
}

static int lua__mask_set(lua_State *L)
{
	uint16_t mask = (uint16_t)luaL_checkinteger(L, 1);
	uint16_t n = luaL_checkinteger(L, 2);
	int bit = lua_toboolean(L, 3);
	if (bit)
		mask |= 1 << (n - 1);
	else
		mask &= ~(1 << (n - 1));
	lua_pushinteger(L, mask);
	return 1;
}

static int lua__mask_get(lua_State *L)
{
	uint16_t mask = luaL_checkinteger(L, 1);
	uint16_t n = luaL_checkinteger(L, 2);
	lua_pushboolean(L, mask & (1 << (n - 1)));
	return 1;
}

static int opencls__sinfo(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"size", lua__scene_size},
		{"set", lua__scene_set},
		{"get", lua__scene_get},
		{"tostring", lua__scene_tostring},
		{"mem_size", lua__scene_memsize},
		{NULL, NULL},
	};
	luaL_newmetatable(L, SINFO_NAME);
	luaL_newlib(L, lmethods);
	lua_setfield(L, -2, "__index");
	return 1;
}

int luaopen_lsinfo(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"new", lua__scene_new},
		{"load", lua__scene_load},
		{"mask_set", lua__mask_set},
		{"mask_get", lua__mask_get},
		{NULL, NULL},
	};
	opencls__sinfo(L);
	luaL_newlib(L, lfuncs);
	return 1;
}
