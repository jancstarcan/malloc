#include "interface.h"

#include <stdio.h>
#include <string.h>

void* malloc(size_t size) {
	if (size == 0)
		return NULL;

	if (size >= MMAP_THRESHOLD) {
		void* p = mm_mmap_alloc(size);
		mm_write_canary(mm_header(p));
		mm_poison_alloc(p);

		mm_add_alloced(size, 1);

		return p;
	}

	size = MM_MAX(size, MM_MIN_PAYLOAD);
	void* p = mm_malloc_block(size);
	mm_write_canary(mm_header(p));
	mm_poison_alloc(p);

	mm_add_alloced(size, 0);

	mm_run_checks();
	return p;
}

void free(void* ptr) {
	if (!ptr)
		return;

	header_t* header = mm_header(ptr);
	mm_check_canary(header);

#ifdef MM_DEBUG
	region_t* reg = mm_get_reg(header);
	void* reg_end = mm_get_reg_end(reg);
	if (((void*)header < reg->start || (void*)header >= reg_end) && !mm_is_mmap(header)) {
		fprintf(stderr, "Ptr is not in the accepted range\n");
		fflush(stderr);
		MM_ABORT();
	}
#endif

	mm_poison_free(ptr);

	if (mm_is_mmap(header)) {
		mm_mmap_free(header);
		return;
	}

	// Double free check
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
	mm_add_to_free(header);

	mm_run_checks();
}

void* realloc(void* ptr, size_t size) {
	if (size == 0) {
		free(ptr);
		return NULL;
	}

	// Realloc acts as malloc if no pointer is provided
	if (!ptr)
		return malloc(size);

	header_t* header = mm_header(ptr);
	size_t old_size = mm_get_size(header);
	size = MM_ALIGN_UP(size);

	if (size == old_size) {
		// No change in size
		return ptr;
	} else if (size < old_size) {
		mm_shrink_block(header, size, 0);

		mm_write_canary(header);
		mm_run_checks();
		return ptr;
	}

	if (mm_grow_block(header, size, 0)) {
		mm_write_canary(header);
		mm_add_alloced(size - old_size, 0);
		mm_run_checks();
		return ptr;
	}

	// In case the next block isn't big enough or isn't free,
	// malloc_block is called
	void* new_ptr = mm_malloc_block(size);
	if (!new_ptr)
		return NULL;

	mm_poison_alloc(new_ptr);

	memcpy(new_ptr, ptr, old_size);
	free(ptr);

	mm_add_alloced(size, 0);
	mm_write_canary(mm_header(new_ptr));
	mm_run_checks();

	return new_ptr;
}

// This is just malloc with memset(0) and a bounds check
void* calloc(size_t size, size_t n) {
	if (size == 0 || n == 0 || size > SIZE_MAX / n)
		return NULL;

	size_t tot_size = MM_ALIGN_UP(size * n);
	if (tot_size < size * n)
		return NULL;

	void* ptr;

	if (tot_size >= MMAP_THRESHOLD) {
		ptr = mm_mmap_alloc(tot_size);
		mm_add_alloced(tot_size, 1);
	} else {
		ptr = mm_malloc_block(tot_size);
		mm_add_alloced(tot_size, 0);
	}

	if (!ptr)
		return NULL;

	memset(ptr, 0, tot_size);
	mm_write_canary(mm_header(ptr));
	mm_run_checks();

	return ptr;
}
