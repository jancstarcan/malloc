#include "interface.h"

#include <stdio.h>

void mm_coalesce_prev(header_t** header_ptr) {
	header_t* h = *header_ptr;
	header_t* prev = h->prev;

	if (!prev || !MM_IS_FREE(prev)) {
		return;
	}

	mm_remove_free(prev);

	size_t size = MM_GET_SIZE(h);
	size_t prev_size = MM_GET_SIZE(prev);

	size_t tot_size = prev_size + MM_METADATA_SIZE + size;
	prev->size = MM_SET_XFREE(tot_size);

	if ((void*)MM_NEXT_HEADER(h) < mm_heap_end) {
		MM_LINK_NEXT_HEADER(prev);
	}

	*header_ptr = prev;
}

void mm_coalesce_next(header_t* h) {
	header_t* next = MM_NEXT_HEADER(h);
	if ((void*)next >= (void*)mm_heap_end || !MM_IS_FREE(next)) {
		return;
	}

	mm_remove_free(next);

	size_t size = MM_GET_SIZE(h);
	size_t next_size = MM_GET_SIZE(next);
	size_t tot_size = size + MM_METADATA_SIZE + next_size;

	h->size = MM_SET_XFREE(tot_size);

	if ((void*)MM_NEXT_HEADER(h) < mm_heap_end) {
		MM_LINK_NEXT_HEADER(h);
	}
}

void mm_shrink_block(header_t* header, size_t size, _Bool is_free) {
	size_t old_size = MM_GET_SIZE(header);
	size_t leftover = old_size - size;

	if (leftover >= MM_MIN_BLOCK_SPLIT) {
		header->size = MM_CLR_FLAGS(size) | (is_free ? MM_FREE_BIT : 0);
		header_t* new_free = MM_NEXT_HEADER(header);

		size_t new_size = leftover - MM_METADATA_SIZE;
		new_free->size = MM_SET_XFREE(new_size);
		new_free->prev = header;

		if ((void*)MM_NEXT_HEADER(new_free) < mm_heap_end) {
			MM_LINK_NEXT_HEADER(new_free);
		}

		mm_coalesce_next(new_free);
		mm_add_to_free(new_free);
	}
}

_Bool mm_grow_block(header_t* h, size_t size, _Bool is_free) {
	size_t old_size = MM_GET_SIZE(h);
	header_t* next = MM_NEXT_HEADER(h);

	if ((void*)next >= (void*)mm_heap_end)
		return 0;

	if (!MM_IS_FREE(next))
		return 0;

	size_t next_size = MM_GET_SIZE(next);
	size_t tot_size = old_size + next_size;
	size_t free_space = tot_size + MM_METADATA_SIZE;

	if (free_space < size)
		return 0;

	mm_remove_free(next);

	if (tot_size - size < MM_MIN_SPLIT) {
		// The entire next block gets absorbed
		mm_poison_alloc_area((void*)next, MM_HEADER_SIZE + next_size);
		h->size = MM_CLR_FLAGS(free_space);
		MM_LINK_NEXT_HEADER(h);
	} else {
		// The next block gets split
		h->size = MM_CLR_FLAGS(size);
		void* poison_start = (uint8_t*)MM_PAYLOAD(h) + old_size;
		mm_poison_alloc_area(poison_start, size - old_size);

		next = MM_NEXT_HEADER(h);
		next->size = MM_SET_XFREE(tot_size - size);

		mm_add_to_free(next);
	}

	h->size |= (is_free ? MM_FREE_BIT : 0);

	return 1;
}

void* mm_malloc_block(size_t size) {
	size = MM_ALIGN_UP(size);

	if (!mm_heap_initialized) {
		if (!mm_init_heap()) {
			return NULL;
		}
	}

	header_t* free_block = mm_find_fit(size);

	if (!free_block) {
		if (!mm_grow_heap()) {
			return NULL;
		} else {
			return mm_malloc_block(size);
		}
	}

	mm_shrink_block(free_block, size, 0);

	return (void*)((uint8_t*)free_block + MM_HEADER_SIZE);
}
