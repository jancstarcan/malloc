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
inline void write_canary(header_t* h) {
	size_t* c = CANARY(h);
	memset(c, CANARY_BYTE, CANARY_SIZE);
}
inline void check_canary(header_t* h) {
	size_t* c = CANARY(h);
	size_t len = ALIGNMENT / sizeof(size_t);
	size_t w = (size_t)-1 / 0xFF * CANARY_BYTE;

	while (len--) {
		if (*c++ != w) {
			fprintf(stderr, "Canry corruption ar %p\n", (void*)h);
			abort();
		}
	}
}
inline void poison_free(void* p) {
	header_t* h = HEADER(p);
	size_t s = GET_SIZE(h);

	// Skips past the next pointer if in release mode
#ifndef DEBUG
	p = (void*)((uint8_t*)p + sizeof(void*));
#endif

	if (s == 0 || (s > heap_size && !IS_MMAP(h))) {
		fprintf(stderr, "Invalid block size %zu\n", s);
		abort();
	}

	memset(p, POISON_FREE_BYTE, s);
}
inline void poison_alloc(void* p) {
	header_t* h = HEADER(p);
	size_t s = GET_SIZE(h);

	if (s == 0 || (s > heap_size && !IS_MMAP(h))) {
		fprintf(stderr, "Invalid block size %zu\n", s);
		fprintf(stderr, "%p\n", p);
		abort();
	}

	memset(p, POISON_ALLOC_BYTE, s);
}
inline void poison_free_area(void* p, size_t s) {
	if (s == 0)
		return;

	// Skips past the next pointer if in release mode
#ifndef DEBUG
	p = (void*)((uint8_t*)p + sizeof(void*));
#endif

	memset(p, POISON_FREE_BYTE, s);
}
inline void poison_alloc_area(void* p, size_t s) {
	if (s == 0)
		return;

	memset(p, POISON_ALLOC_BYTE, s);
}
#else
inline void write_canary(header_t* h) {}
inline void check_canary(header_t* h) {}
inline void poison_free(void* p) {}
inline void poison_alloc(void* p) {}
inline void poison_free_area(void* p, size_t s) {}
inline void poison_alloc_area(void* p, size_t s) {}
#endif

void heap_check(void) {
	header_t* cur = (header_t*)heap_start;
	while ((void*)cur < heap_end) {
		size_t size = GET_SIZE(cur);
		footer_t* footer = FOOTER(cur);

		assert(size % ALIGNMENT == 0);
		assert(footer->size == size);

		cur = NEXT_HEADER(cur);
	}
}

void free_check(void) {
	header_t* cur = free_list;
	while (cur) {
		assert(IS_FREE(cur));
		cur = GET_NEXT(cur);
	}
}
