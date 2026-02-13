#include "interface.h"

#include <stdio.h>

void mm_coalesce_prev(header_t** header_ptr) {
	header_t* cur = *header_ptr;
	header_t* prev = (uint8_t*)cur >= (uint8_t*)mm_heap_start + MM_MIN_BLOCK_SIZE ? MM_PREV_HEADER(cur) : NULL;

	if (!prev || !MM_IS_FREE(prev))
		return;

	if (!mm_remove_free(prev)) {
#ifdef MM_DEBUG
		fprintf(stderr, "Block is marked as free but isn't in the free list\n");
		MM_ABORT();
#endif
	}

	size_t cur_size = MM_GET_SIZE(cur);
	size_t prev_size = MM_GET_SIZE(prev);

	size_t tot_size = prev_size + MM_CANARY_SIZE + MM_FOOTER_SIZE + MM_HEADER_SIZE + cur_size;
	prev->size = MM_SET_XFREE(tot_size);
	MM_FOOTER(prev)->size = tot_size;

	*header_ptr = prev;
}

void mm_coalesce_next(header_t* cur) {
	header_t* next = MM_NEXT_HEADER(cur);

	if ((uint8_t*)next + MM_HEADER_SIZE > (uint8_t*)mm_heap_end || !MM_IS_FREE(next))
		return;

	if (!mm_remove_free(next)) {
#ifdef MM_DEBUG
		fprintf(stderr, "Block is marked as free but isn't in the free list\n");
		MM_ABORT();
#endif
	}

	size_t cur_size = MM_GET_SIZE(cur);
	size_t next_size = MM_GET_SIZE(next);

	size_t tot_size = cur_size + MM_CANARY_SIZE + MM_FOOTER_SIZE + MM_HEADER_SIZE + next_size;
	cur->size = MM_SET_XFREE(tot_size);
	MM_FOOTER(cur)->size = tot_size;
}

void mm_shrink_block(header_t* header, size_t size) {
	size_t old_size = MM_GET_SIZE(header);
	size_t leftover = old_size - size;

	if (leftover >= MM_MIN_BLOCK_SIZE) {
		header->size = MM_CLR_FLAGS(size);
		footer_t* footer = MM_FOOTER(header);
		footer->size = size;

		header_t* new_free = (header_t*)((uint8_t*)footer + MM_FOOTER_SIZE);
		size_t new_size = leftover - MM_HEADER_SIZE - MM_CANARY_SIZE - MM_FOOTER_SIZE;
		new_free->size = MM_SET_XFREE(new_size);
		MM_FOOTER(new_free)->size = new_size;

		mm_coalesce_next(new_free);
		mm_add_to_free(new_free);
	}
}

_Bool mm_grow_block(header_t* h, size_t size) {
	size_t old_size = MM_GET_SIZE(h);

	if ((void*)((uint8_t*)MM_FOOTER(h) + MM_MIN_BLOCK_SIZE) >= mm_heap_end)
		return 0;

	header_t* next = MM_NEXT_HEADER(h);
	size_t next_size = MM_GET_SIZE(next);
	size_t tot_size = old_size + next_size;

	if (!MM_IS_FREE(next) || tot_size < size)
		return 0;

	mm_remove_free(next);

	if (tot_size - size < MM_MIN_PAYLOAD) {
		// The entire next block gets absorbed
		tot_size = MM_CANARY_SIZE + MM_FOOTER_SIZE + tot_size + MM_HEADER_SIZE;
		mm_poison_alloc_area((void*)next, MM_HEADER_SIZE + next_size);
		h->size = MM_CLR_FLAGS(tot_size);
		MM_FOOTER(h)->size = tot_size;
	} else {
		// The next block gets split
		h->size = MM_CLR_FLAGS(size);
		MM_FOOTER(h)->size = size;
		void* new_start = (uint8_t*)MM_PAYLOAD(h) + old_size;
		mm_poison_alloc_area(new_start, size - old_size);

		next_size = tot_size - size;
		next = MM_NEXT_HEADER(h);
		next->size = MM_SET_XFREE(next_size);
		MM_FOOTER(next)->size = next_size;

		mm_add_to_free(next);
	}

	return 1;
}

void* mm_malloc_block(size_t size) {
	if (!mm_heap_initialized)
		if (!mm_init_heap())
			return NULL;

	header_t* free_block = mm_find_fit(size);

	if (!free_block) {
		if (!mm_grow_heap())
			return NULL;
		else
			return mm_malloc_block(size);
	}

	size_t free_size = MM_GET_SIZE(free_block);

	// Check for whether the block should be split
	if (free_size - size >= MM_MIN_BLOCK_SIZE) {
		// Split case
		size_t block_size = MM_HEADER_SIZE + size + MM_CANARY_SIZE + MM_FOOTER_SIZE;
		header_t* new_free = (header_t*)((uint8_t*)free_block + block_size);
		size_t new_size = free_size - block_size;

		new_free->size = MM_SET_FREE(new_size);
		MM_FOOTER(new_free)->size = new_size;

		mm_add_to_free(new_free);
	} else {
		// No split case
		size = MM_GET_SIZE(free_block);
	}

	free_block->size = MM_CLR_FLAGS(size);
	MM_FOOTER(free_block)->size = size;

	return (void*)((uint8_t*)free_block + MM_HEADER_SIZE);
}
