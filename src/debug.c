#include "interface.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void debug_test(void) {
	heap_check();
	free_check();
}

#ifdef ENABLE_CANARIES
inline void write_canary(Header* h) {
	uint8_t* c = CANARY(h);
	memset(c, CANARY_BYTE, CANARY_SIZE);
}
inline void check_canary(Header* h) {
	uint8_t* c = CANARY(h);
	for (size_t i = 0; i < CANARY_SIZE; i++)
		assert(*c++ == CANARY_BYTE);
}
inline void poison_free(void* p) {
	Header* h = HEADER(p);
	size_t s = GET_SIZE(h);

	if (s == 0 || (s > heap_size && !IS_MMAP(h))) {
		fprintf(stderr, "Invalid block size %zu\n", s);
		abort();
	}

	memset(p, POISON_FREE_BYTE, s);
}
inline void poison_alloc(void* p) {
	Header* h = HEADER(p);
	size_t s = GET_SIZE(h);

	if (s == 0 || (s > heap_size && !IS_MMAP(h))) {
		fprintf(stderr, "Invalid block size %zu\n", s);
		abort();
	}

	memset(p, POISON_ALLOC_BYTE, s);
}
inline void poison_free_area(void* p, size_t s) {
	if (s == 0)
		return;

	memset(p, POISON_ALLOC_BYTE, s);
}
inline void poison_alloc_area(void* p, size_t s) {
	if (s == 0)
		return;

	memset(p, POISON_ALLOC_BYTE, s);
}
#else
inline void write_canary(Header* h) {}
inline void check_canary(Header* h) {}
inline void poison_free(void* p) {}
inline void poison_alloc(void* p) {}
inline void poison_free_area(void* p, size_t s) {}
inline void poison_alloc_area(void* p, size_t s) {}
#endif

void heap_check(void) {
	Header* cur = (Header*)heap_start;
	while ((void*)cur < heap_end) {
		size_t size = GET_SIZE(cur);
		size_t footer_value = *FOOTER(cur);

		assert(size % ALIGNMENT == 0);
		assert(footer_value == size);

		cur = NEXT_HEADER(cur);
	}
}

void free_check(void) {
	Header* cur = free_list;
	while (cur) {
		assert(IS_FREE(cur));
		cur = cur->next;
	}
}
