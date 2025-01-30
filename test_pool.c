#include <stdio.h>
#include <pthread.h>
#include "poolalloc.h"

void alloc_thread(pool_t* pool) {
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = pool_alloc(pool);
        if (ptrs[i] == NULL) {
            printf("Failed to allocate chunk %d\n", i);
            return;
        }
    }
    for (int i = 0; i < 10; i++) {
        printf("ptrs[%d] = %p\n", i, ptrs[i]);
        pool_free(pool, ptrs[i]);
    }
}

int main() {
    pool_t* pool = pool_create(16, 10*10);
    if (pool == NULL) {
        printf("Failed to create pool\n");
        return 1;
    }

    pthread_t threads[10];
    for (int i = 0; i < 10; i++) {
        pthread_create(&threads[i], NULL, (void* (*)(void*))alloc_thread, pool);
    }

    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nThreads are done, checking state!\n\n");

    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = pool_alloc(pool);
        if (ptrs[i] == NULL) {
            printf("Failed to allocate chunk %d\n", i);
            return 1;
        }
    }

    for (int i = 0; i < 100; i++) {
        for (int j = i+1; j < 100; j++) {
            if (ptrs[i] == ptrs[j]) {
                printf("Duplicate pointers found at %d and %d\n", i, j);
                return 1;
            }
        }
    }

    for (int i = 0; i < 100; i++) {
        printf("ptrs[%d] = %p\n", i, ptrs[i]);
        pool_free(pool, ptrs[i]);
    }

    printf("Everything seems OK!\n");

    pool_destroy(pool);
    return 0;
}