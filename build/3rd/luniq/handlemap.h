#ifndef HANDLE_MAP_H
#define HANDLE_MAP_H

#include <stddef.h>
#define HANDLE_UD_NULL ((void *)(ptrdiff_t)1)
typedef unsigned int handleid;

struct handlemap;

struct handlemap * handlemap_init();
void handlemap_exit(struct handlemap *);

handleid handlemap_new(struct handlemap *, void *ud);
void * handlemap_grab(struct handlemap *, handleid id);
void * handlemap_release(struct handlemap *, handleid id);
int handlemap_list(struct handlemap *, int n, handleid * result);

#endif
