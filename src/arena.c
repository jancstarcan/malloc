#include "interface.h"

#include <limits.h>
#include <stdio.h>
#include <sys/mman.h>

arena_t mm_arena;
_Bool mm_arena_initialized = 0;

static inline _Bool check_mmap(void* ptr) {
	if (ptr == (void*)-1) {
#ifdef MM_DEBUG
		perror("mmap");
#endif
		return 0;
	}

	return 1;
}

_Bool mm_init_arena(void) {
	arena_t* arena = &mm_arena;
	void* new = mmap(NULL, MM_REG_SIZE * 2, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!check_mmap(new)) {
		return 0;
	}
	new = mm_reg_align(new);

	region_t* reg = (region_t*)new;
	header_t* h = mm_get_reg_header(reg);
	h->size = mm_set_xfree(MM_REG_FREE);
	h->prev = NULL;
	mm_set_prev(h, NULL);
	mm_set_next(h, NULL);

	reg->arena = arena;
	reg->next = NULL;
	reg->start = (void*)h;

	arena->free = (free_list_t){0};

	arena->start = (region_t*)reg;
	arena->tail = (region_t*)reg;

	mm_add_to_free(h);
	mm_arena_initialized = 1;

	return 1;
}

_Bool mm_grow_arena(arena_t* arena) {
	size_t new_size;
	if (arena->reg_count >= sizeof(size_t) * CHAR_BIT) {
		new_size = MM_REG_CAP;
	} else if (MM_REG_SIZE > (SIZE_MAX >> arena->reg_count)) {
		new_size = MM_REG_CAP;
	} else {
		new_size = MM_REG_SIZE << arena->reg_count;
		if (new_size > MM_REG_CAP) {
			new_size = MM_REG_CAP;
		}
	}

	void* new = mmap(NULL, new_size + MM_REG_SIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!check_mmap(new)) {
		return 0;
	}
	new = mm_reg_align(new);

	region_t* reg = (region_t*)new;
	region_t* prev = arena->tail;
	void* mmap_end = (void*)((uint8_t*)new + new_size);

	while ((void*)reg < mmap_end) {
		reg->arena = arena;
		reg->start = mm_get_reg_header(reg);

		header_t* h = (header_t*)reg->start;
		h->size = mm_set_xfree(MM_REG_FREE);
		h->prev = NULL;
		mm_set_prev(h, NULL);
		mm_set_next(h, NULL);

		mm_add_to_free(h);

		if (prev) prev->next = reg;
		prev = reg;
		reg = mm_get_reg_end(reg);
	}

	arena->tail = prev;
	if (!arena->start) arena->start = (region_t*)new;

	arena->reg_count++;

	return 1;
}

void* mm_mmap_alloc(size_t size) {
	size = MM_ALIGN_UP(size);
	size_t tot_size = size + MM_METADATA_SIZE;

	void* new = mmap(NULL, tot_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!check_mmap(new)) {
		return 0;
	}

	header_t* header = (header_t*)new;
	header->size = mm_set_mmap(mm_clr_free(size));

	return (void*)((uint8_t*)new + MM_HEADER_SIZE);
}

void mm_mmap_free(header_t* header) {
	size_t size = mm_get_size(header) + MM_METADATA_SIZE;

	if (munmap((void*)header, size) == -1) {
#ifdef MM_DEBUG
		perror("mmap");
#endif
	}
}
