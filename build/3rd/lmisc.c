#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bresenham/bresenham.h"
#include <ctype.h>
// #include <lua.h>
// #include <lauxlib.h>

#if (LUA_VERSION_NUM < 502 && !defined(luaL_newlib))
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

#if defined(WIN32) || defined(_WIN32)
# include <windows.h>
# include <time.h>

static int gettimeofday(struct timeval *tp, void *tzp)
{
	time_t clock;
	struct tm tm; 
	SYSTEMTIME wtm;

	GetLocalTime(&wtm);
	tm.tm_year     = wtm.wYear - 1900;
	tm.tm_mon     = wtm.wMonth - 1;
	tm.tm_mday     = wtm.wDay;
	tm.tm_hour     = wtm.wHour;
	tm.tm_min     = wtm.wMinute;
	tm.tm_sec     = wtm.wSecond;
	tm.tm_isdst    = -1; 
	clock = mktime(&tm);
	tp->tv_sec = clock;
	tp->tv_usec = wtm.wMilliseconds * 1000;

	return 0;
}
#else
# include <sys/time.h>
#endif

struct bh_udata {
	lua_State *L;
	int idx;
	int ref;
	size_t max;
};

static void init_bh_udata(struct bh_udata *udata,
			  lua_State *L,
			  int max, int ref)
{
	udata->L = L;
	udata->max = max;
	udata->ref = ref;
	udata->idx = -1;
}

static int pushpos2lua(void *data, int x, int y)
{
	struct bh_udata *udata = (struct bh_udata *)data;
	lua_State *L = udata->L;
	if (udata->max > 0 && udata->idx + 1 > udata->max) {
		return BH_STOP;
	}
	if (udata->ref != LUA_NOREF) {
		int top = lua_gettop(L);
		lua_rawgeti(L, LUA_REGISTRYINDEX, udata->ref);
		lua_pushnumber(L, x);
		lua_pushnumber(L, y);
		if (lua_pcall(L, 2, 1, 0) == 0) {
			if (!lua_toboolean(L, -1)) {
				lua_settop(L, top);
				return BH_STOP;
			}
		} else {
			return BH_STOP;
		}
		lua_settop(L, top);
	}
	udata->idx++;

	lua_newtable(L);
	lua_pushinteger(L, (lua_Integer)x);
	lua_rawseti(L, -2, 1);
	lua_pushinteger(L, (lua_Integer)y);
	lua_rawseti(L, -2, 2);
	lua_rawseti(L, -2, udata->idx + 1);

	return BH_CONTINUE;
}

static int lua__bresenham(lua_State *L)
{
	int ret;
	struct bh_udata udata;
	int ref = LUA_NOREF;
	int sx = luaL_checkint(L, 1);
	int sy = luaL_checkint(L, 2);
	int ex = luaL_checkint(L, 3);
	int ey = luaL_checkint(L, 4);
	int max = luaL_optint(L, 5, -1);
	if (lua_type(L, 6) == LUA_TFUNCTION) {
		ref = luaL_ref(L, LUA_REGISTRYINDEX);
	}
	init_bh_udata(&udata, L, max, ref);
	lua_newtable(L);
	ret = bresenham_line(sx, sy, ex, ey, pushpos2lua, &udata);
	lua_pushboolean(L, ret == 0);
	if (ref != LUA_NOREF) {
		luaL_unref(L, LUA_REGISTRYINDEX, ref);
	}
	return 2;
}

static int lua__gettimeofday(lua_State *L)
{
	struct timeval tv; 
	if (gettimeofday(&tv, NULL) != 0) {
		return 0; 
	} 
	lua_pushnumber(L, tv.tv_sec);
	lua_pushnumber(L, tv.tv_usec);
	return 2;
}

static unsigned short checksum(const char *str, int count)
{
	/**
	 * Compute Internet Checksum for "count" bytes
	 * beginning at location "addr".
	 */
	register long sum = 0;
	char *addr = (char *)str;

	while( count > 1 )  {
		/*  This is the inner loop */
		sum += * (unsigned short *) addr++;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if( count > 0 )
		sum += * (unsigned char *) addr;

	/*  Fold 32-bit sum to 16 bits */
	while (sum>>16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

static int lua__checksum(lua_State *L)
{
	size_t sz;
	const char *str = luaL_checklstring(L, 1, &sz);
	unsigned short csum = checksum(str, sz);
	lua_pushinteger(L, (lua_Integer)csum);
	return 1;
}

#ifndef PREFIX_XXT
# define PREFIX_XXT ".xxt"
#endif

#ifndef PREFIX_RC4
# define PREFIX_RC4 ".rc4"
#endif

#ifndef ENCODE_KEY
# define ENCODE_KEY "975478165e40c51b"
#endif

static int lua__source_loadstring(lua_State *L)
{
	int ret;
	size_t sz;
	const char *str = luaL_checklstring(L, 1, &sz);
	const char *path = luaL_checkstring(L, 2);

	size_t ssz;
	const char *source = NULL;
	char *out = NULL;

	size_t pxxt_len = strlen(PREFIX_XXT);
	size_t prc4_len = strlen(PREFIX_RC4);

	if (memcmp(str, PREFIX_XXT, pxxt_len) == 0) {
		lua_pushnil(L);
		lua_pushfstring(L, "not support %s", PREFIX_XXT);
		return 2;
	} else if (memcmp(str, PREFIX_RC4, prc4_len) == 0) {
		rc4_state_t *s = NULL;
		out = malloc(sz - prc4_len);
		s = rc4_create((const uint8_t *)ENCODE_KEY, (int)prc4_len);
		rc4_crypt(s, (const uint8_t *)str + prc4_len, (uint8_t *)out, sz - prc4_len);
		rc4_destroy(s);
		source = out;
	} else {
		source = str;
		ssz = sz;
	}
	ret = luaL_loadbuffer(L, source, sz, path);
	free(out);
	if (ret == 0) {
		return 1;
	}
	lua_pushnil(L);
	lua_insert(L, -2);
	return 2;
}

static int luaG__xpcall (lua_State *L) 
{
        int status;
        luaL_checktype(L, 1, LUA_TFUNCTION);
        luaL_checktype(L, 2, LUA_TFUNCTION);

        lua_pushvalue(L, 2); /*f, err, ..., err*/
        lua_insert(L, 1);  /* err, f, err, ...; put error function under function to be called */
        lua_remove(L, 3); /* err, f, ...*/
        status = lua_pcall(L, lua_gettop(L) - 2, LUA_MULTRET, 1); 
        lua_pushboolean(L, (status == 0));
        lua_replace(L, 1); 
        return lua_gettop(L);  /* return status + all results */
}


/*************************************************************************/
static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static char *decoding_table = NULL;
static int mod_table[] = {0, 2, 1};


char *misc_base64_encode(const char *data, size_t input_length, size_t *output_length) {

	char *encoded_data = 0;
	int i, j;

	uint32_t octet_a;
	uint32_t octet_b;
	uint32_t octet_c;
	uint32_t triple;

	*output_length = 4 * ((input_length + 2) / 3);

	encoded_data = malloc(*output_length);
	if (encoded_data == NULL) return NULL;

	for (i = 0, j = 0; i < input_length;) {

		octet_a = i < input_length ? (unsigned char)data[i++] : 0;
		octet_b = i < input_length ? (unsigned char)data[i++] : 0;
		octet_c = i < input_length ? (unsigned char)data[i++] : 0;

		triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

		encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
	}

	for (i = 0; i < mod_table[input_length % 3]; i++)
		encoded_data[*output_length - 1 - i] = '=';

	return encoded_data;
}


void build_decoding_table() {
	int i;
	decoding_table = malloc(256);
	for (i = 0; i < 64; i++)
		decoding_table[(unsigned char) encoding_table[i]] = i;
}


unsigned char *misc_base64_decode(const char *data, size_t input_length, size_t *output_length) {

	int i , j;
	unsigned char *decoded_data = 0;
	uint32_t sextet_a;
	uint32_t sextet_b;
	uint32_t sextet_c;
	uint32_t sextet_d;
	uint32_t triple;

	if (decoding_table == NULL) build_decoding_table();

	if (input_length % 4 != 0) return NULL;

	*output_length = input_length / 4 * 3;
	if (data[input_length - 1] == '=') (*output_length)--;
	if (data[input_length - 2] == '=') (*output_length)--;

	decoded_data = malloc(*output_length);
	if (decoded_data == NULL) return NULL;

	for (i = 0, j = 0; i < input_length;) {

		sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[(int)data[i++]];
		sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[(int)data[i++]];
		sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[(int)data[i++]];
		sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[(int)data[i++]];

		triple = (sextet_a << 3 * 6)
		+ (sextet_b << 2 * 6)
		+ (sextet_c << 1 * 6)
		+ (sextet_d << 0 * 6);

		if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
		if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
		if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
	}

	return decoded_data;
}


void base64_cleanup() {
	free(decoding_table);
}

static int lua__base64_encode(lua_State *L)
{
	size_t in_len =  0;
	size_t out_len = 0;
	const char * inStr = (const char * ) luaL_checklstring(L, 1, &in_len);
	char * out = misc_base64_encode(inStr, in_len, &out_len);
	if(0 == out)
	{
		return 0;
	}
	lua_pushlstring(L, (char*)out, out_len);
	free(out);
	return 1;
}


static int lua__base64_decode(lua_State *L)
{
	size_t in_len =  0;
	size_t i = 0;
	size_t out_len = 0;
	char c;
	const char *  inStr =  (const char * )luaL_checklstring(L, 1, &in_len);
	unsigned char * out;
	if(in_len == 0)
	{
		return luaL_argerror(L, 1, "bad input string,len=0");
	}

	for(i = 0; i < in_len; ++i)
	{
		c = inStr[i];
		if( !  (isalnum(c) || (c == '+') || (c == '/') || (c == '=')) )
			return luaL_argerror(L, 1, "non base64 str");
	}

	out = misc_base64_decode(inStr, in_len, &out_len);
	if(0 == out)
	{
		return 0;
	}
	lua_pushlstring(L, (char*)out, out_len);
	free(out);
	return 1;
}


/*************************************************************************/


int luaopen_misc(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"gettimeofday", lua__gettimeofday},
		{"bresenham", lua__bresenham},
		{"checksum", lua__checksum},
		{"loadstring", lua__source_loadstring},
		{"base64_encode", lua__base64_encode},
		{"base64_decode", lua__base64_decode},
		{NULL, NULL},
	};
        lua_pushcfunction(L, luaG__xpcall);
        lua_setglobal(L, "xpcall");
	luaL_newlib(L, lfuncs);
	return 1;
}
