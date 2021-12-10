/*
 ** $Id: ldblib.c,v 1.104 2005/12/29 15:32:11 roberto Exp $
 ** Interface from Lua to its debug API
 ** See Copyright Notice in lua.h
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

/*
#include "lua.h"
#include "lauxlib.h"
*/
#include "uthash.h"

#if LUA_VERSION_NUM < 502 && (!defined(luaL_newlib))
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

static const char KEY_PROF = 'p';

struct lprof_cost_t;

struct lprof_record_t {
	char* name;
	char* source;
	char* what;
	int linedefined;

	struct lprof_cost_t* childs; 
	clock_t time;	/* cost */
	int  count; /* count */	

	int id;	/* key */
	UT_hash_handle hh; /* makes this structure hashable */

	/* internal */
	clock_t start;
};

struct lprof_cost_t
{
	clock_t time;
	int count;
	struct lprof_record_t *record;

	int id; /* key */
	UT_hash_handle hh; /* makes this structure hashable */
};

struct profiler_t {
	char output[FILENAME_MAX];
	struct lprof_record_t *records;
	int traceback;
	int deep;
};

static void lprof_dump( struct profiler_t *profiler);

struct lprof_record_t* lprof_getrecord( struct profiler_t *profiler,  lua_Debug *ar,  int f, int create )
{
	struct lprof_record_t *retval;
	int id = 0;
	if (f == 0 ) return 0;
	id = f;

	/* find it */
	HASH_FIND_INT(profiler->records, &id, retval); 

	if (retval == 0 && create == 1) 
	{
		/* first time, add it */
		retval = malloc(sizeof(struct lprof_record_t));
		retval->id = id;
		retval->time = 0;
		retval->count = 0;
		retval->name = ar->name ? strdup( ar->name ) : 0;
		retval->source = ar->source ? strdup( ar->source) : 0;
		retval->what = ar->what ? strdup( ar->what ) : 0;
		retval->linedefined = ar->linedefined;
		retval->childs = 0;
		HASH_ADD_INT(profiler->records, id, retval);
	};
	return retval;
}

static struct lprof_cost_t* lprof_getchildcost(struct lprof_record_t* parent,
					struct lprof_record_t* child,
					int create)
{	
	struct lprof_cost_t* retval;
	int id = 0;
	if ( child == 0 ) return 0;
	id = (int)child->id;

	/* find it */
	HASH_FIND_INT(parent->childs, &id, retval); 
	if ( retval == 0 && create == 1)
	{
		/* first time, add it */
		retval = malloc(sizeof(struct lprof_cost_t));
		retval->id = id;
		retval->count = 0;
		retval->time = 0;
		retval->record = child;
		HASH_ADD_INT(parent->childs, id, retval);
	}
	return retval;
}

static struct lprof_record_t * lprof_getfromstack(lua_State*L,
						  struct profiler_t *profiler,
						  int level)
{
	struct lprof_record_t *retval = 0;
	if ( profiler->deep > 0 )
	{
		lua_getref(L,profiler->traceback);
		lua_pushinteger(L,level);
		lua_rawget(L,-2);
		retval = (struct lprof_record_t*)lua_topointer(L,-1);
		lua_pop(L,1);
	}
	return retval;

}

static void lprof_hookf(lua_State *L, lua_Debug *ar)
{
	/* ar was initialized in luaD_callhook, just ar.i_ci was set */
	struct profiler_t *profiler = 0;
	struct lprof_record_t * parent = 0;
	struct lprof_record_t * self = 0;
	struct lprof_cost_t *cost = 0;
	int f = 0;
	int delta;

	lua_pushlightuserdata(L, (void *)&KEY_PROF);
	lua_rawget(L, LUA_REGISTRYINDEX);
	lua_pushlightuserdata(L, L);
	lua_rawget(L, -2);

	if (lua_islightuserdata(L, -1)) {
		profiler = lua_touserdata(L,-1);

		switch( ar->event )
		{
			case LUA_HOOKCALL:
				/* get function */
				lua_getinfo(L,"Snf",ar);
				f = (int)lua_topointer(L,-1);
				lua_pop(L,1);					
				self = lprof_getrecord(profiler, ar, f, 1);
				self->start = clock();
				self->count++;

				/* put into traceback */
				profiler->deep++;
				lua_getref(L,profiler->traceback);				
				lua_pushinteger(L,profiler->deep);
				lua_pushlightuserdata(L,self);
				lua_rawset(L,-3);
				lua_pop(L,1);
				break;
			case LUA_HOOKRET:
				self = lprof_getfromstack(L,profiler,profiler->deep);
				if ( self != 0 )
				{
					delta = clock() - self->start;
					self->time += delta;

					parent = lprof_getfromstack(L,profiler,profiler->deep -1 );

					if ( parent != 0)
					{
						cost = lprof_getchildcost( parent, self, 1);
						cost->count++;
						cost->time += delta;
					};
					profiler->deep--;
				};
				break;
		} 
	}
}

static lua_State *lprof_getthread (lua_State *L, int *arg) {
	if (lua_isthread(L, 1)) {
		*arg = 1;
		return lua_tothread(L, 1);
	}
	else {
		*arg = 0;
		return L;
	}
}

static void lprof_gethooktable (lua_State *L) {
	lua_pushlightuserdata(L, (void *)&KEY_PROF);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_createtable(L, 0, 1);
		lua_pushlightuserdata(L, (void *)&KEY_PROF);
		lua_pushvalue(L, -2);
		lua_rawset(L, LUA_REGISTRYINDEX);
	}
}

static int lprof_start(lua_State *L)
{
	/* thread filename */
	struct profiler_t *profiler = 0;
	int arg;
	lua_State *L1 = lprof_getthread(L,&arg);
	if (lua_isnoneornil(L,arg+1)) {
		return luaL_error(L, LUA_QL("start")
				" should be provided filename");		
	}
	else {
		lua_sethook(L1, lprof_hookf, LUA_MASKCALL | LUA_MASKRET, 0);
	}

	profiler = (struct profiler_t*)malloc( sizeof(struct profiler_t)) ;
	profiler->records = 0;
	strncpy(profiler->output,lua_tostring(L,arg+1),FILENAME_MAX);
	profiler->deep = 0;

	lprof_gethooktable(L1);
	lua_pushstring(L1,"traceback");
	lua_newtable(L1);
	lua_pushvalue(L1,-1);
	profiler->traceback = lua_ref(L1,-1);
	lua_rawset(L1,-3);

	/* set profiler */
	lua_pushlightuserdata(L1,L1);
	lua_pushlightuserdata(L1,profiler);
	lua_rawset(L1,-3);

	lua_pop(L1,1); /* remove hook table */
	return 0;
}

static void lprof_destroy( lua_State *L, struct profiler_t * profiler )
{
	struct lprof_record_t *cur;
	struct lprof_cost_t *cost;
	for ( cur = profiler->records; cur != 0;) 
	{
		struct lprof_record_t *deleting = cur;	
		for (cost = cur->childs; cost != 0; )
		{
			struct lprof_cost_t *deleting = cost;
			cost = cost->hh.next;
			free(deleting);
		}
		cur = cur->hh.next;		
		free( deleting );
	}
	lua_unref(L,profiler->traceback);
}

static int lprof_stop(lua_State *L)
{
	struct profiler_t *profiler = 0;
	int arg;
	lua_State *L1 = lprof_getthread(L, &arg);
	lua_sethook(L1,NULL,0,0); /* turn off hooks */	

	/* get it */
	lprof_gethooktable(L1);
	lua_pushlightuserdata(L1,L1);
	lua_rawget(L1,-2);
	profiler = (void*)lua_topointer(L,-1);
	lua_pop(L1,1);

	lprof_dump( profiler );

	lprof_destroy( L1, profiler );

	/* remove it */
	lua_pushlightuserdata(L1,L1);
	lua_pushnil(L1);
	lua_rawset(L1,-3);

	lua_pop(L1,1); /* remove hook table */
	
	return 0;
}

static int lprof_cost_sort(struct lprof_record_t *a, struct lprof_record_t *b)
{
	return (a->time - b->time);
}

static void lprof_pretty(struct lprof_record_t *record, char *buf )
{
	sprintf( buf, "  %s:%s:%s:%d  ",record->what ? record->what : "",
			record->name ? record->name : "" ,
			record->source ? record->source : "",
			record->linedefined );
}

static void lprof_pad(char *buf, char c, int size )
{
	char *temp;
	int pad = 0;	
	int len = 0;
	len = strlen(buf);
	if (size < len ) return;
	pad = (size - len)/2;
	temp = strdup( buf );
	memset( buf, c, pad );
	sprintf( buf + pad, "%s", temp );
	memset( buf + pad + len, c, size - pad - len );
	buf[size] ='\0';
	free(temp);
}

static void lprof_dump( struct profiler_t *profiler)
{
	struct lprof_record_t *cur;
	FILE* fout ;
	if ( profiler == 0 ) return;		
	fout = fopen(profiler->output,"w+");
	HASH_SORT(profiler->records, lprof_cost_sort);
	for ( cur = profiler->records; cur != 0; cur = cur->hh.next) 
	{
		char buf[4096];
		struct lprof_cost_t *cost;
		clock_t child_time = 0;
		for( cost = cur->childs; cost != 0; cost = cost->hh.next)
		{
			child_time += cost->time;
		}
		fprintf(fout,"\n");
		lprof_pretty( cur, buf );		
		lprof_pad( buf, '-', 80);
		fprintf(fout,"%s\n",buf);
		fprintf(fout,"%30s: %d\n","Call count",cur->count);
		fprintf(fout,"%30s: %.3fs\n","Total time",cur->time/1000.0);
		fprintf(fout,"%30s: %.3fs\n","Time spent in children", child_time/1000.0);
		fprintf(fout,"%30s: %.3fs\n","Time spent in self",(cur->time - child_time)/1000.0);
		fprintf(fout,"%30s: %.5fs/call\n","Time spent per call",cur->time/(1000.0*cur->count));
		fprintf(fout,"\n");

		for (cost = cur->childs; cost != 0; cost = cost->hh.next)
		{
			lprof_pretty( cost->record, buf );
			fprintf(fout,"Child: %-35s called %8d times. Took %.3fs\n", buf, cost->count, cost->time/1000.0); 
		}
	}
	fclose(fout);
}

static const luaL_Reg dblib[] = {
	{"start", lprof_start},
	{"stop", lprof_stop},
	{NULL, NULL}
};


int luaopen_lprofile (lua_State *L) {
	luaL_newlib(L, dblib);
	return 1;
}

