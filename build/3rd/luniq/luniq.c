#include <stdlib.h>
#include <stdio.h>

#include "handlemap.h"

#if LUA_VERSION_NUM < 502 && (!defined(luaL_newlib))
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

#define CLS_UNIQ "uniq{cls}"
#define LIST_MAX_SIZE 1024 * 1024 * 1024

#define CHECK_UNIQ(L, idx)\
	(*(struct handlemap **) luaL_checkudata(L, idx, CLS_UNIQ))

#define LUNIQ_LUA_BIND_META(L, type_t, ptr, mname) do {             \
	type_t **my__p = lua_newuserdata(L, sizeof(void *));        \
	*my__p = ptr;                                               \
	luaL_getmetatable(L, mname);                                \
	lua_setmetatable(L, -2);                                    \
} while(0)

/* #define ENABLE_XXX_DEBUG */
#ifdef ENABLE_XXX_DEBUG
# define UNIQ_DLOG(fmt, ...) fprintf(stderr, "<luniq>" fmt "\n", ##__VA_ARGS__)
#else
# define UNIQ_DLOG(...)
#endif

static int lua__uniq_new(lua_State *L)
{
	struct handlemap * h = handlemap_init();
	if (h == NULL) {
		return 0;
	}
	LUNIQ_LUA_BIND_META(L, struct handlemap, h, CLS_UNIQ);
	return 1;
}

static int lua__uniq_new_id(lua_State *L)
{
	struct handlemap * h = CHECK_UNIQ(L, 1);
	handleid id = handlemap_new(h, HANDLE_UD_NULL);
	lua_pushinteger(L, id);
	return 1;
}

static int lua__uniq_release(lua_State *L)
{
	struct handlemap * h = CHECK_UNIQ(L, 1);
	handleid id = (handleid)luaL_checkinteger(L, 2);
	handlemap_release(h, id);
	return 0;
}

static int lua__uniq_inuse(lua_State *L)
{
	struct handlemap * h = CHECK_UNIQ(L, 1);
	handleid id = (handleid)luaL_checkinteger(L, 2);
	void *p = handlemap_grab(h, id);
	lua_pushboolean(L, p != NULL);
	return 1;
}

static int lua__uniq_list(lua_State *L)
{
	int i;
	int outsz = 0;
	handleid slist[1024];
	handleid *list = (handleid *)slist;
	int insz = sizeof(slist)/sizeof(slist[0]);
	struct handlemap * h = CHECK_UNIQ(L, 1);
	do {
		outsz = handlemap_list(h, insz, list);
		if (outsz < insz) {
			break;
		}
		if (list != slist) {
			free(list);
		}
		insz *= 2;
		if (insz > LIST_MAX_SIZE) {
			return 0;
		}
		list = malloc(sizeof(handleid) * insz);
	} while(1);

	lua_newtable(L);
	for (i = 0; i < outsz; i++) {
		lua_pushinteger(L, (lua_Integer)list[i]);
		lua_rawseti(L, -2, i + 1);
	}
	
	if (list != slist) {
		free(list);
	}
	return 1;
}


static int lua__uniq_gc(lua_State *L)
{
	struct handlemap * h = CHECK_UNIQ(L, 1);
	handlemap_exit(h);
	UNIQ_DLOG("release handlemap");
	return 0;
}

static int opencls__luniq(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"new", lua__uniq_new_id},
		{"release", lua__uniq_release},
		{"list", lua__uniq_list},
		{"inuse", lua__uniq_inuse},
		{NULL, NULL},
	};
	luaL_newmetatable(L, CLS_UNIQ);
	lua_newtable(L);
	luaL_register(L, NULL, lmethods);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction (L, lua__uniq_gc);
	lua_setfield (L, -2, "__gc");
	return 1;
}

int luaopen_luniq(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"new", lua__uniq_new},
		{NULL, NULL},
	};
	opencls__luniq(L);
	luaL_newlib(L, lfuncs);
	return 1;
}
