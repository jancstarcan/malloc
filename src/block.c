#include "interface.h"

#include <stdio.h>
#include <stdlib.h>

void coalesce_prev(Header** header_ptr) {
	Header* cur = *header_ptr;
	Header* prev = (uint8_t*)cur >= (uint8_t*)heap_start + MIN_BLOCK_SIZE
					   ? PREV_HEADER(cur)
					   : NULL;

	if (!prev || !IS_FREE(prev))
		return;

	if (!remove_free(prev)) {
#ifdef DEBUG
		fprintf(stderr, "Block is marked as free but isn't in the free list\n");
		fflush(stderr);
		abort();
#endif
	}

	size_t cur_size = GET_SIZE(cur);
	size_t prev_size = GET_SIZE(prev);

	size_t tot_size =
		prev_size + CANARY_SIZE + FOOTER_SIZE + HEADER_SIZE + cur_size;
	prev->size = SET_XFREE(tot_size);
	*FOOTER(prev) = tot_size;

	*header_ptr = prev;
}

void coalesce_next(Header* cur) {
	Header* next = NEXT_HEADER(cur);

	if ((uint8_t*)next + HEADER_SIZE > (uint8_t*)heap_end || !IS_FREE(next))
		return;

	if (!remove_free(next)) {
#ifdef DEBUG
		fprintf(stderr, "Block is marked as free but isn't in the free list\n");
		fflush(stderr);
		abort();
#endif
	}

	size_t cur_size = GET_SIZE(cur);
	size_t next_size = GET_SIZE(next);

	size_t tot_size =
		cur_size + CANARY_SIZE + FOOTER_SIZE + HEADER_SIZE + next_size;
	cur->size = SET_XFREE(tot_size);
	*FOOTER(cur) = tot_size;
}

void shrink_block(Header* header, size_t size) {
	size_t old_size = GET_SIZE(header);
	size_t leftover = old_size - size;

	if (leftover >= MIN_BLOCK_SIZE) {
		header->size = CLR_FLAGS(size);
		size_t* footer = FOOTER(header);
		*footer = size;

		Header* new_free = (Header*)((uint8_t*)footer + FOOTER_SIZE);
		size_t new_size = leftover - HEADER_SIZE - CANARY_SIZE - FOOTER_SIZE;
		new_free->size = SET_XFREE(new_size);
		*FOOTER(new_free) = new_size;

		coalesce_next(new_free);
		add_to_free(new_free);
	}
}

_Bool try_grow_in_place(Header* header, size_t size) {
	size_t old_size = GET_SIZE(header);

	if ((void*)((uint8_t*)FOOTER(header) + MIN_BLOCK_SIZE) >= heap_end)
		return 0;

	Header* next = NEXT_HEADER(header);
	size_t next_size = GET_SIZE(next);
	size_t tot_size = old_size + next_size;

	if (!IS_FREE(next) || tot_size < size)
		return 0;

	remove_free(next);

	if (tot_size - size < MIN_PAYLOAD) {
		// The entire next block gets absorbed
		tot_size = HEADER_SIZE + tot_size + CANARY_SIZE + FOOTER_SIZE;
		header->size = CLR_FLAGS(tot_size);
		*FOOTER(header) = tot_size;
	} else {
		// The next block gets split
		header->size = CLR_FLAGS(size);
		*FOOTER(header) = size;

		next_size = tot_size - size;
		next = NEXT_HEADER(header);
		next->size = SET_XFREE(next_size);
		*FOOTER(next) = next_size;

		add_to_free(next);
	}

	return 1;
}

void* malloc_block(size_t size) {
	if (!heap_initialized)
		if (!init_heap())
			return NULL;

	Header* free_block = find_fit(size);

	if (!free_block) {
		if (!grow_heap())
			return NULL;
		else
			return malloc_block(size);
	}

	size_t free_size = GET_SIZE(free_block);

	// Check for whether the block should be split
	if (free_size - size >= MIN_BLOCK_SIZE) {
		// Split case
		size_t block_size = HEADER_SIZE + size + CANARY_SIZE + FOOTER_SIZE;
		Header* new_free = (Header*)((uint8_t*)free_block + block_size);
		size_t new_size = free_size - block_size;

		new_free->size = SET_FREE(new_size);
		*FOOTER(new_free) = new_size;

		add_to_free(new_free);
	} else {
		// No split case
		size = GET_SIZE(free_block);
	}

	free_block->size = CLR_FLAGS(size);
	*FOOTER(free_block) = size;

	return (void*)((uint8_t*)free_block + HEADER_SIZE);
}
