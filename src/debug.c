#include "interface.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

void mm_debug_test(void) {
	arena_t* arena = &mm_arena;
	region_t* reg;

	reg = arena->start;
	while (reg) {
		mm_reg_check(reg);
		reg = reg->next;
	}

	mm_free_check(&arena->free);
}

#ifdef MM_ENABLE_CANARIES
inline void mm_write_canary(header_t* h) {
	size_t* c = mm_canary(h);
	memset(c, MM_CANARY_BYTE, MM_CANARY_SIZE);
}
inline void mm_check_canary(header_t* h) {
	size_t* c = mm_canary(h);
	size_t len = MM_ALIGNMENT / sizeof(size_t);
	size_t w = (size_t)-1 / 0xFF * MM_CANARY_BYTE;

	while (len--) {
		if (*c++ != w) {
			fprintf(stderr, "Canary corruption ar %p\n", (void*)h);
			MM_ABORT();
		}
	}
}
#else
inline void mm_write_canary(header_t* h) {}
inline void mm_check_canary(header_t* h) {}
#endif

#ifdef MM_ENABLE_POISONING
inline void mm_poison_free(void* p) {
	header_t* h = mm_header(p);
	size_t s = mm_get_size(h);

	// Skips past the next pointer if in release mode
#ifndef MM_DEBUG
	p = (void*)((uint8_t*)p + sizeof(void*));
	s -= sizeof(void*);
#endif

	if (s == 0 || (s > MM_REG_FREE && !mm_is_mmap(h))) {
		fprintf(stderr, "Invalid block size %zu\n", s);
		MM_ABORT();
	}

	memset(p, MM_POISON_FREE_BYTE, s);
}
inline void mm_poison_alloc(void* p) {
	header_t* h = mm_header(p);
	size_t s = mm_get_size(h);

	if (s == 0 || (s > MM_REG_FREE && !mm_is_mmap(h))) {
		fprintf(stderr, "Invalid block size %zu\n", s);
		fprintf(stderr, "%p\n", p);
		MM_ABORT();
	}

	memset(p, MM_POISON_ALLOC_BYTE, s);
}
inline void mm_poison_free_area(void* p, size_t s) {
	if (s == 0)
		return;

	// Skips past the next pointer if in release mode
#ifndef MM_DEBUG
	p = (void*)((uint8_t*)p + sizeof(void*));
	s -= sizeof(void*);
#endif

	memset(p, MM_POISON_FREE_BYTE, s);
}
inline void mm_poison_alloc_area(void* p, size_t s) {
	if (s == 0)
		return;

	memset(p, MM_POISON_ALLOC_BYTE, s);
}
#else
inline void mm_poison_free(void* p) {}
inline void mm_poison_alloc(void* p) {}
inline void mm_poison_free_area(void* p, size_t s) {}
inline void mm_poison_alloc_area(void* p, size_t s) {}
#endif

void mm_reg_check(region_t* reg) {
	header_t* cur = (header_t*)reg->start;
	void* reg_end = mm_get_reg_end(reg);
	header_t* next;
	for (;;) {
		size_t size = mm_get_size(cur);

		assert((uintptr_t)cur % MM_ALIGNMENT == 0);
		assert(size % MM_ALIGNMENT == 0);
		next = mm_next_header(cur);
		if ((void*)next >= reg_end) {
			break;
		}

		assert(next->prev == cur);
		cur = next;
	}
}

void mm_free_check(free_list_t* free) {
	header_t** list = free->start;
	for (size_t i = 0; i < MM_BIN_COUNT; i++) {
		header_t* cur = list[i];
		while (cur) {
			assert(mm_is_free(cur));
			cur = mm_get_next(cur);
		}
	}
}
