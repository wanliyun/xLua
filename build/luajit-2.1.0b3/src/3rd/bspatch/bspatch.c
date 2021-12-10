/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2012 Matthew Endsley
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "./bzip2-1.0.6/bzlib.h"
#include "./bzip2-1.0.6/blocksort.c"
#include "./bzip2-1.0.6/huffman.c"
#include "./bzip2-1.0.6/crctable.c"
#include "./bzip2-1.0.6/randtable.c"
#include "./bzip2-1.0.6/compress.c"
#include "./bzip2-1.0.6/decompress.c"
#include "./bzip2-1.0.6/bzlib.c"


#include "bspatch.h"

static int64_t offtin(uint8_t *buf)
{
	int64_t y;

	y=buf[7]&0x7F;
	y=y*256;y+=buf[6];
	y=y*256;y+=buf[5];
	y=y*256;y+=buf[4];
	y=y*256;y+=buf[3];
	y=y*256;y+=buf[2];
	y=y*256;y+=buf[1];
	y=y*256;y+=buf[0];

	if(buf[7]&0x80) y=-y;

	return y;
}

int bspatch(const uint8_t* old, int64_t oldsize, uint8_t* newp, int64_t newsize, struct bspatch_stream* stream)
{
	uint8_t buf[8];
	int64_t oldpos,newpos;
	int64_t ctrl[3];
	int64_t i;

	oldpos=0;newpos=0;
	while(newpos<newsize) {
		/* Read control data */
		for(i=0;i<=2;i++) {
			if (stream->read(stream, buf, 8))
				return -1;
			ctrl[i]=offtin(buf);
		};

		/* Sanity-check */
		if(newpos+ctrl[0]>newsize)
			return -1;

		/* Read diff string */
		if (stream->read(stream, newp + newpos, ctrl[0]))
			return -1;

		/* Add old data to diff string */
		for(i=0;i<ctrl[0];i++)
			if((oldpos+i>=0) && (oldpos+i<oldsize))
				newp[newpos+i]+=old[oldpos+i];

		/* Adjust pointers */
		newpos+=ctrl[0];
		oldpos+=ctrl[0];

		/* Sanity-check */
		if(newpos+ctrl[1]>newsize)
			return -1;

		/* Read extra string */
		if (stream->read(stream, newp + newpos, ctrl[1]))
			return -1;

		/* Adjust pointers */
		newpos+=ctrl[1];
		oldpos+=ctrl[2];
	};

	return 0;
}

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include <fcntl.h>

static int bz2_read(const struct bspatch_stream* stream, void* buffer, int length)
{
	int n;
	int bz2err;
	BZFILE* bz2;

	bz2 = (BZFILE*)stream->opaque;
	n = BZ2_bzRead(&bz2err, bz2, buffer, length);
	if (n != length)
		return -1;

	return 0;
}

int readFile(const char * fileName,uint8_t ** ptr,size_t * size,char * errorBuf)
{
	FILE* f = fopen(fileName,"rb");
	if(!f)
	{
		sprintf(errorBuf, "ERROR cannot openfile [%s]",fileName);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	*ptr = (uint8_t*)malloc(*size+1);
	if(! (*ptr))
	{
		sprintf(errorBuf,"ERRORmalloc error file=%s",fileName);
		return -2;
	}
	fseek(f, 0,SEEK_SET);
	if(1 != fread(*ptr, *size, 1, f))
	{
		sprintf(errorBuf,"ERROR read file=%s",fileName);
		return -3;
	}
	fclose(f);
	return 0;
}

static int bspatch_do_patch(uint8_t *old, size_t oldsize,const char * patchfile,uint8_t ** newp, size_t * newSize, char * errorBuf)
{
	int bz2err,ret;
	uint8_t header[24] = {0};
	FILE * f;
	BZFILE* bz2;
	struct bspatch_stream stream;

	/**************************检查patchfile*****************************/
	/* Open patch file */
	if ((f = fopen(patchfile, "rb")) == NULL)
	{
		sprintf(errorBuf, "fopen(%s)", patchfile);
		return -1;
	}

	/* Read header */
	if (fread(header, 1, 24, f) != 24) 
	{
		fclose(f);
		sprintf(errorBuf, "Corrupt patch 1 (%s)", patchfile);
		return -1;
	}

	/* Check for appropriate magic */
	if (memcmp(header, "ENDSLEY/BSDIFF43", 16) != 0)
	{
		fclose(f);
		sprintf(errorBuf, "Corrupt patch 2 (%s)", patchfile);
		return -1;
	}

	/* Read lengths from header */
	*newSize=offtin(header+16);
	if((int64_t)(*newSize) < 0)
	{
		fclose(f);
		sprintf(errorBuf, "Corrupt patch 3 (%s)", patchfile);
		return -1;
	}
	
	/**************************patch*****************************/
	*newp=malloc(*newSize+1);
	if(*newp==0)
	{
		fclose(f);
		sprintf(errorBuf,  "Failed to malloc(%d)", (int)(*newSize+1));
		return -1;
	}
	
	if (0 == (bz2 = BZ2_bzReadOpen(&bz2err, f, 0, 0, NULL, 0)))
	{
		free(*newp);
		fclose(f);
		sprintf(errorBuf, "BZ2_bzReadOpen, bz2err=%d", bz2err);
		return -1;
	}
	
	stream.read = bz2_read;
	stream.opaque = bz2;
	ret = bspatch(old, oldsize, *newp, *newSize, &stream);
	BZ2_bzReadClose(&bz2err, bz2);
	fclose(f);
	
	if(ret)
	{
		free(*newp);
		sprintf(errorBuf, "bspatch failed bz2err=%d",bz2err);
		return -1;
	}
	return 0;
}

static int lua__bsppatch(lua_State *L)
{
	FILE * f;
	int ret;
	uint8_t *old, *newp;
	size_t oldSize;
	size_t newSize;

	
	const char * oldfile;
	const char * newfile;
	const char * patchfile;
	char errorBuf[1024] = {0};
	
	oldfile = luaL_checkstring(L, 1);
	newfile = luaL_checkstring(L, 2);
	patchfile = luaL_checkstring(L, 3);
	
	
	/**************************读取oldfile*****************************/
	if(0 != readFile(oldfile, &old, &oldSize,errorBuf))
	{
		return luaL_error(L, errorBuf);
	}
	
	/**************************打Patch*****************************/
	if(0 != bspatch_do_patch(old, oldSize, patchfile, &newp, &newSize, errorBuf))
	{
		return luaL_error(L, errorBuf);
	}

	/**************************写入newfile*****************************/
	f = fopen(newfile, "wb");
	if(f == 0)
	{
		free(newp);
		return luaL_error(L, "open newfile(%s) failed", newfile);
	}
	
	ret = fwrite(newp,1,newSize,f);
	free(newp);
	fclose(f);
	if( ret != newSize )
	{
		return luaL_error(L, "write newfile(%s) failed", newfile);
	}

	lua_pushboolean(L, 1);
	return 1;
}

static int lua__bsppatch2(lua_State *L)
{
	uint8_t *newp;
	size_t oldSize,newSize;

	const char * oldfileStr = luaL_checklstring(L, 1, &oldSize);
	const char * patchFile = luaL_checkstring(L, 2);
	char errorBuf[1024] = {0};

	/**************************打Patch*****************************/
	if(0 != bspatch_do_patch((uint8_t*)oldfileStr, oldSize, patchFile,  &newp, &newSize, errorBuf))
	{
		return luaL_error(L, errorBuf);
	}

	lua_pushlstring(L, (const char*)newp, newSize );
	free(newp);
	return 1;
}

int luaopen_lbspatch(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"patch", 	lua__bsppatch},
		{"patch2", 	lua__bsppatch2},
		{NULL, NULL},
	};
	luaL_newlib(L, lfuncs);
	return 1;
}
