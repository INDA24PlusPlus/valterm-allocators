#define _GNU_SOURCE
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "poolalloc.h"

#define PAGE_SIZE 4096
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define PAGEALIGN(x) ALIGN(x, PAGE_SIZE)

struct chunk_t {
    struct chunk_t* next;
};

struct pool_t {
    size_t mapping_size;
    size_t num_chunks;
    size_t chunk_size;
    struct chunk_t* head;
    pthread_mutex_t lock;
};

pool_t* pool_create(size_t chunk_size, size_t num_chunks) {
    size_t alloc_size = sizeof(pool_t) + num_chunks * chunk_size;
    pool_t* pool = (pool_t*)mmap(NULL, PAGEALIGN(alloc_size), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pool == MAP_FAILED) {
        return NULL;
    }

    pool->mapping_size = alloc_size;
    pool->num_chunks = num_chunks;
    pool->chunk_size = chunk_size;

    struct chunk_t* c = (struct chunk_t*)(pool + 1);
    pool->head = c;

    for (size_t i = 0; i < num_chunks - 1; i++) {
        c->next = (struct chunk_t*)((char*)c + chunk_size);
        c = c->next;
    }

    c->next = NULL;

    pthread_mutex_init(&pool->lock, NULL);

    return pool;
}

/* int pool_expand(pool_t* pool, size_t num_chunks) {
    size_t new_size = pool->mapping_size + num_chunks * pool->chunk_size;
    if (PAGEALIGN(new_size) != PAGEALIGN(pool->mapping_size)) {
        if (mremap(pool, PAGEALIGN(pool->mapping_size), PAGEALIGN(new_size), 0) != pool) {
            // Something bad happened
            return -1;
        }
    }
    pool->mapping_size = new_size;


}
 */
void* pool_alloc(pool_t* pool) {
    pthread_mutex_lock(&pool->lock);

    if (pool->head == NULL) {
        pthread_mutex_unlock(&pool->lock);
        return NULL;
    }

    struct chunk_t* c = pool->head;
    pool->head = c->next;

    pthread_mutex_unlock(&pool->lock);

    return c;
}

void pool_free(pool_t* pool, void* ptr) {
    pthread_mutex_lock(&pool->lock);

    struct chunk_t* c = (struct chunk_t*)ptr;
    c->next = pool->head;
    pool->head = c;

    pthread_mutex_unlock(&pool->lock);
}

void pool_destroy(pool_t* pool) {
    pthread_mutex_destroy(&pool->lock);
    munmap(pool, pool->mapping_size);
}