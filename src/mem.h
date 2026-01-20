#include <stddef.h>

#ifndef MALLOC_PUBLIC_HEADER
#define MALLOC_PUBLIC_HEADER
void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t size, size_t n);

void dump_heap(void);
void dump_free_list(void);
void print_stats(void);
#endif
