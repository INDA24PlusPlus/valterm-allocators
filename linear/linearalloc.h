#ifndef __LINEARALLOC_H__
#define __LINEARALLOC_H__

#include <stddef.h>

void linearalloc_init();

void* malloc(size_t size);

void* calloc(size_t nmemb, size_t size);

void* realloc(void* ptr, size_t size);

void free(void* ptr);

#endif