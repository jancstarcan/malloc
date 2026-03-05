#include "interface.h"

#include <stdio.h>
#include <string.h>

void* malloc(size_t size) {
	if (size == 0)
		return NULL;

	size = MM_ALIGN_UP(size);

	if (MM_SLAB_ALIGN_UP(size) <= MM_SLAB_THRESHOLD) {
		size = MM_SLAB_ALIGN_UP(size);
		mm_slab_malloc(size, &mm_arena);
	} else if (size >= MMAP_THRESHOLD) {
		void* p = mm_mmap_alloc(size);
		return p;
	} else {
		MM_ABORT();
	}

	size = MM_MAX(size, MM_MIN_PAYLOAD);
	void* p = mm_malloc_block(size);

	return p;
}

void free(void* ptr) {
	if (!ptr)
		return;

	header_t* header = mm_header(ptr);

	if (mm_is_mmap(header)) {
		mm_mmap_free(header);
		return;
	}

#ifdef MM_DEBUG
	region_t* reg = mm_get_reg(header);
	void* reg_end = mm_get_reg_end(reg);
	void* reg_start = mm_get_reg_start(reg);
	if (((void*)header < reg_start || (void*)header >= reg_end)) {
		fprintf(stderr, "Ptr is not in the accepted range\n");
		fflush(stderr);
		MM_ABORT();
	}
#endif

	if (mm_is_free(header)) {
#ifdef MM_DEBUG
		fprintf(stderr, "Double free detected\n");
		fflush(stderr);
		MM_ABORT();
#else
		return;
#endif
	}

	header->size = mm_set_free(header->size);
	mm_coalesce_prev(&header);
	mm_coalesce_next(header);
}

void* realloc(void* ptr, size_t size) {
	if (size == 0) {
		free(ptr);
		return NULL;
	}

	if (!ptr)
		return malloc(size);

	header_t* header = mm_header(ptr);
	size_t old_size = mm_get_size(header);
	size = MM_ALIGN_UP(size);

	if (size == old_size) {
		return ptr;
	} else if (size < old_size) {
		mm_shrink_block(header, size, 0);
		return ptr;
	}

	if (mm_grow_block(header, size, 0)) {
		return ptr;
	}

	void* new_ptr = mm_malloc_block(size);
	if (!new_ptr)
		return NULL;

	memcpy(new_ptr, ptr, old_size);
	free(ptr);

	return new_ptr;
}

void* calloc(size_t size, size_t n) {
	if (size == 0 || n == 0 || size > SIZE_MAX / n)
		return NULL;

	size_t tot_size = MM_ALIGN_UP(size * n);
	if (tot_size < size * n)
		return NULL;

	void* ptr;

	if (tot_size >= MMAP_THRESHOLD) {
		ptr = mm_mmap_alloc(tot_size);
	} else {
		ptr = mm_malloc_block(tot_size);
	}

	if (!ptr)
		return NULL;

	memset(ptr, 0, tot_size);
	return ptr;
}
