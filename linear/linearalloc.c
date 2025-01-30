#define _GNU_SOURCE
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "linearalloc.h"

#define ALIGNMENT 32
#define ALIGN(x) (((x) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define GET_CHUNK(usrdata) ((struct chunk_t*)((char*)usrdata - offsetof(struct chunk_t, fd)))
#define GET_USRDATA(chunk) ((void*)((char*)chunk + offsetof(struct chunk_t, fd)))
#define NEXT_CHUNK(chunk) ((struct chunk_t*)((char*)chunk + chunk->size + sizeof(struct chunk_t)))
#define IS_LATEST_CHUNK(chunk) (NEXT_CHUNK(chunk) == state.top)

#define NFREEBINS 16
#define BININCREMENT ALIGNMENT
#define BINIDX(size) (((size) / BININCREMENT) - 1)

//#define LOG(fmt, ...) heap_log(fmt, ##__VA_ARGS__)
#define LOG(fmt, ...)

void heap_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[DBG] ");
    vprintf(fmt, args);
    va_end(args);
}

struct chunk_t {
    char pad[8];
    size_t size;
    // Userdata follows, fields are only used if chunk is free
    struct chunk_t* fd;
    struct chunk_t* bk;
};

struct state_t {
    pthread_mutex_t lock;

    void* mmap_base;

    // Pointer to the top (allocated) chunk of the heap (highest address)
    struct chunk_t* top;

    // Array of free bins, each bin contains a list of free chunks of a certain size
    struct chunk_t* free[NFREEBINS];
};

static struct state_t state = {0};

void linearalloc_init() {
    pthread_mutex_init(&state.lock, NULL);

    // Resizing the heap is for losers
    state.mmap_base = mmap(NULL, 0x100000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (state.mmap_base == MAP_FAILED) {
        perror("mmap");
        abort();
    }

    state.top = state.mmap_base;
    //state.top->prev_size = 0;

    // Occupy the first chunk so no one can allocate it then free it (lol)
    void* _ = malloc(16);
}

void* malloc(size_t size) {
    if (state.mmap_base == NULL) {
        linearalloc_init();
    }

    pthread_mutex_lock(&state.lock);

    // Make sure size is aligned
    size = ALIGN(size);

    // Find a free chunk in the bins
    int bin = BINIDX(size);
    if (bin >= 0 && bin < NFREEBINS) {
        // Check if there is a free chunk in the bin
        struct chunk_t* bin_head = state.free[bin];
        if (bin_head != NULL) {
            // Found a free chunk, remove it from the bin
            struct chunk_t* bin_tail = bin_head->bk;

            LOG("Allocating chunk @ %p from bin %d\n", bin_head, bin);

            if (bin_head == bin_tail) {
                // Bin is now empty
                state.free[bin] = NULL;
            } else {
                // Unlink the first chunk
                bin_tail->fd = bin_head->fd;
                bin_head->fd->bk = bin_tail;
                state.free[bin] = bin_head->fd;
            }

            pthread_mutex_unlock(&state.lock);

            return GET_USRDATA(bin_head);
        }
    }

    // No free chunk in the bins, allocate a new chunk

    // Remember the previous top chunk
    struct chunk_t* prev = state.top;

    prev->size = size;

    // Move top pointer to the next chunk, our new chunk
    state.top = (struct chunk_t*)((unsigned long)prev + size + sizeof(struct chunk_t));

    state.top->size = 0;
    // state.top->prev_size = prev->size;

    pthread_mutex_unlock(&state.lock);

    return GET_USRDATA(prev);
}

void free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    pthread_mutex_lock(&state.lock);

    struct chunk_t* chunk = GET_CHUNK(ptr);

    if (IS_LATEST_CHUNK(chunk)) {
        // Chunk is adjacent to the top chunk, move top pointer back
        LOG("Freeing top chunk @ %p\n", chunk);
        state.top = (struct chunk_t*)((unsigned long)state.top - chunk->size - sizeof(struct chunk_t));
        state.top->size = 0;
        pthread_mutex_unlock(&state.lock);
        return;
    }

    // Insert chunk into the free list
    int bin = BINIDX(chunk->size);
    if (bin < 0 || bin >= NFREEBINS) {
        // Chunk is too big to be put into a bin
        LOG("Freeing chunk @ %p, too big for bins\n", chunk);
        pthread_mutex_unlock(&state.lock);
        return;
    }

    LOG("Freeing chunk @ %p, bin %d\n", chunk, bin);

    struct chunk_t* bin_head = state.free[bin];

    if (bin_head == NULL) {
        // Bin is empty, insert chunk as the first element
        chunk->fd = chunk;
        chunk->bk = chunk;
        state.free[bin] = chunk;
    } else {
        // Bin is not empty, insert chunk at the end
        struct chunk_t* bin_tail = bin_head->bk;

        chunk->fd = bin_head;
        chunk->bk = bin_tail;
        bin_head->bk = chunk;
        bin_tail->fd = chunk;
        state.free[bin] = chunk;
    }

    pthread_mutex_unlock(&state.lock);
}

void* calloc(size_t nmemb, size_t size) {
    if (nmemb*size == 0) {
        return NULL;
    }
    void* ptr = malloc(nmemb * size);
    if (ptr == NULL) {
        return NULL;
    }

    memset(ptr, 0, nmemb * size);

    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    pthread_mutex_lock(&state.lock);

    struct chunk_t* chunk = GET_CHUNK(ptr);
    size_t old_size = chunk->size;
    if (IS_LATEST_CHUNK(chunk)) {
        state.top->size = size;
        pthread_mutex_unlock(&state.lock);
        return ptr;
    }
    pthread_mutex_unlock(&state.lock);

    void* newptr = malloc(size);
    if (newptr == NULL) {
        return NULL;
    }

    memcpy(newptr, ptr, old_size);

    free(ptr);

    return newptr;
}