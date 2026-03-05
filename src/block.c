#include "interface.h"

#include <stdio.h>

void mm_coalesce_prev(header_t** header_ptr) {
	header_t* h = *header_ptr;
	header_t* prev = h->prev;

	region_t* reg = mm_get_reg(h);
	void* reg_end = mm_get_reg_end(reg);

	if (!prev || !mm_is_free(prev)) {
		return;
	}

	mm_remove_free(prev);

	size_t size = mm_get_size(h);
	size_t prev_size = mm_get_size(prev);

	size_t tot_size = prev_size + MM_METADATA_SIZE + size;
	prev->size = mm_set_xfree(tot_size);

	if ((void*)mm_next_header(prev) < reg_end) {
		mm_link_next_header(prev);
	}

	*header_ptr = prev;
}

void mm_coalesce_next(header_t* h) {
	region_t* reg = mm_get_reg(h);
	void* reg_end = mm_get_reg_end(reg);

	header_t* next = mm_next_header(h);

	if ((void*)next >= reg_end) {
		return;
	}

	if (!mm_is_free(next)) {
		return;
	}

	mm_remove_free(next);

	size_t size = mm_get_size(h);
	size_t next_size = mm_get_size(next);
	size_t tot_size = size + MM_METADATA_SIZE + next_size;

	h->size = mm_set_xfree(tot_size);

	if ((void*)mm_next_header(h) < reg_end) {
		mm_link_next_header(h);
	}
}

void mm_shrink_block(header_t* h, size_t size, _Bool is_free) {
	region_t* reg = mm_get_reg(h);
	void* reg_end = mm_get_reg_end(reg);

	size_t old_size = mm_get_size(h);
	size_t leftover = old_size - size;

	if (leftover >= MM_MIN_BLOCK_SPLIT) {
		h->size = mm_clr_flags(size) | (is_free ? MM_FREE_BIT : 0);
		header_t* new_free = mm_next_header(h);

		size_t new_size = leftover - MM_METADATA_SIZE;
		new_free->size = mm_set_xfree(new_size);
		new_free->prev = h;

		if ((void*)mm_next_header(new_free) < reg_end) {
			mm_link_next_header(new_free);
		}

		mm_coalesce_next(new_free);
		mm_add_to_free(new_free);
	} else {
		h->size = mm_get_size(h) | (is_free ? MM_FREE_BIT : 0);
	}
}

_Bool mm_grow_block(header_t* h, size_t size, _Bool is_free) {
	region_t* reg = mm_get_reg(h);
	void* reg_end = mm_get_reg_end(reg);

	size_t old_size = mm_get_size(h);
	header_t* next = mm_next_header(h);

	if ((void*)next >= (void*)reg_end)
		return 0;

	if (!mm_is_free(next))
		return 0;

	size_t next_size = mm_get_size(next);
	size_t tot_size = old_size + next_size;
	size_t free_space = tot_size + MM_METADATA_SIZE;

	if (free_space < size)
		return 0;

	mm_remove_free(next);

	if (free_space - size < MM_MIN_SPLIT) {
		h->size = mm_clr_flags(free_space);
		mm_link_next_header(h);
	} else {
		h->size = mm_clr_flags(size);

		next = mm_next_header(h);
		next->size = mm_set_xfree(tot_size - size);
		next->prev = h;

		if ((void*)mm_next_header(next) < reg_end) {
			mm_link_next_header(next);
		}

		mm_add_to_free(next);
	}

	h->size |= (is_free ? MM_FREE_BIT : 0);

	return 1;
}

void* mm_malloc_block(size_t size) {
	if (!mm_arena_initialized) {
		if (!mm_init_arena()) {
			return NULL;
		}
	}

	header_t* free_block = mm_find_fit(size, &mm_arena);

	if (!free_block) {
		if (!mm_grow_arena(&mm_arena)) {
			return NULL;
		} else {
			return mm_malloc_block(size);
		}
	}

	mm_shrink_block(free_block, size, 0);
	return (void*)((uint8_t*)free_block + MM_HEADER_SIZE);
}
