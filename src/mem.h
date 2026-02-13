#include <stddef.h>

#ifndef MM_PUBLIC_HEADER
#define MM_PUBLIC_HEADER
void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t size, size_t n);

void mm_print_alloced(void);
void mm_print_free(void);
void mm_print_stats(void);
#endif
