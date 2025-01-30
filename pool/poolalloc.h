#ifndef __POOLALLOC_H__
#define __POOLALLOC_H__

#include <stddef.h>

struct pool_t;

typedef struct pool_t pool_t;

pool_t* pool_create(size_t chunk_size, size_t num_chunks);

void* pool_alloc(pool_t* pool);

void pool_free(pool_t* pool, void* ptr);

void pool_destroy(pool_t* pool);

#endif