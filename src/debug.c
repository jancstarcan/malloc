#include "interface.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

void debug_test(void) {
	heap_check();
	free_check();
}

#ifdef ENABLE_CANARIES
inline void write_canary(header_t* h) {
	size_t* c = MM_CANARY(h);
	memset(c, MM_CANARY_BYTE, MM_CANARY_SIZE);
}
inline void check_canary(header_t* h) {
	size_t* c = MM_CANARY(h);
	size_t len = MM_ALIGNMENT / sizeof(size_t);
	size_t w = (size_t)-1 / 0xFF * MM_CANARY_BYTE;

	while (len--) {
		if (*c++ != w) {
			fprintf(stderr, "Canary corruption ar %p\n", (void*)h);
			abort();
		}
	}
}
inline void poison_free(void* p) {
	header_t* h = MM_HEADER(p);
	size_t s = MM_GET_SIZE(h);

	// Skips past the next pointer if in release mode
#ifndef MM_DEBUG
	p = (void*)((uint8_t*)p + sizeof(void*));
	s -= sizeof(void*);
#endif

	if (s == 0 || (s > heap_size && !MM_IS_MMAP(h))) {
		fprintf(stderr, "Invalid block size %zu\n", s);
		abort();
	}

	memset(p, MM_POISON_FREE_BYTE, s);
}
inline void poison_alloc(void* p) {
	header_t* h = MM_HEADER(p);
	size_t s = MM_GET_SIZE(h);

	if (s == 0 || (s > heap_size && !MM_IS_MMAP(h))) {
		fprintf(stderr, "Invalid block size %zu\n", s);
		fprintf(stderr, "%p\n", p);
		abort();
	}

	memset(p, MM_POISON_ALLOC_BYTE, s);
}
inline void poison_free_area(void* p, size_t s) {
	if (s == 0)
		return;

	// Skips past the next pointer if in release mode
#ifndef MM_DEBUG
	p = (void*)((uint8_t*)p + sizeof(void*));
	s -= sizeof(void*);
#endif

	memset(p, MM_POISON_FREE_BYTE, s);
}
inline void poison_alloc_area(void* p, size_t s) {
	if (s == 0)
		return;

	memset(p, MM_POISON_ALLOC_BYTE, s);
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
		size_t size = MM_GET_SIZE(cur);
		footer_t* footer = MM_FOOTER(cur);

		assert(size % MM_ALIGNMENT == 0);
		assert(footer->size == size);

		cur = MM_NEXT_HEADER(cur);
	}
}

void free_check(void) {
	for (size_t i = 0; i < MM_BIN_COUNT; i++) {
		header_t* cur = free_lists[i];
		while (cur) {
			assert(MM_IS_FREE(cur));
			cur = MM_GET_NEXT(cur);
		}
	}
}
