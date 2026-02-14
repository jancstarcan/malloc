#include "interface.h"

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

void* mm_heap_start;
void* mm_heap_end;
size_t mm_heap_size = 0;
_Bool mm_heap_initialized = 0;

#ifdef MM_DEBUG
_Static_assert(!(MM_ALIGNMENT & 0x1), "MM_ALIGNMENT must be even");
#endif

// Allocates the first INITIAL_HEAP_SIZE bytes
_Bool mm_init_heap(void) {
	mm_heap_size = MM_INITIAL_HEAP_SIZE;
	uintptr_t brk = (uintptr_t)sbrk(0);

	if (brk == (uintptr_t)-1) {
#ifdef MM_DEBUG
		perror("sbrk");
#endif
		return 0;
	}

	size_t mis = brk & (MM_ALIGNMENT - 1);
	if (mis) {
		if (sbrk(MM_ALIGNMENT - mis) == (void*)-1) {
#ifdef MM_DEBUG
			perror("sbrk");
#endif
			return 0;
		}
	}

	mm_heap_start = sbrk(0);

	if (mm_heap_start == (void*)-1 || sbrk(mm_heap_size) == (void*)-1) {
#ifdef MM_DEBUG
		perror("sbrk");
#endif
		return 0;
	}

	mm_heap_end = sbrk(0);

	size_t payload = mm_heap_size - MM_METADATA_SIZE;

	header_t* h = (header_t*)mm_heap_start;
	h->size = MM_SET_XFREE(payload);
	h->prev = NULL;
	MM_SET_NEXT(h, NULL);
	MM_SET_PREV(h, NULL);
	mm_add_to_free(h);

	mm_poison_free(MM_PAYLOAD(h));
	mm_heap_initialized = 1;

	return 1;
}

// Doubles heap size
_Bool mm_grow_heap(void) {
	if (mm_heap_size > SIZE_MAX / 2)
		return 0;

	void* old_end = mm_heap_end;
	if (sbrk(mm_heap_size) == (void*)-1) {
#ifdef MM_DEBUG
		perror("sbrk");
#endif
		return 0;
	}

	mm_heap_end = sbrk(0);

	header_t* last_header;
	header_t* next_header = mm_heap_start;
	while ((void*)next_header < mm_heap_end) {
		last_header = next_header;
		next_header = MM_NEXT_HEADER(next_header);
	}
	void* payload;

	// If the last block is free it gets extended
	// Otherwise a new one is created
	if (MM_IS_FREE(last_header)) {
		mm_remove_free(last_header);
		size_t new_size = mm_heap_size + MM_GET_SIZE(last_header);
		last_header->size = MM_SET_XFREE(new_size);
		mm_add_to_free(last_header);
		payload = MM_PAYLOAD(last_header);
	} else {
		header_t* new_header = (header_t*)old_end;
		size_t new_size = mm_heap_size - MM_METADATA_SIZE;
		new_header->size = MM_SET_XFREE(new_size);
		new_header->prev = last_header;
		mm_add_to_free(new_header);
		payload = MM_PAYLOAD(new_header);
	}

	mm_heap_size *= 2;
	mm_poison_free(payload);
	mm_write_canary(MM_HEADER(payload));

	return 1;
}

// Allocates the requested size directly with mmap
// should only be used on big chunks
void* mm_mmap_alloc(size_t size) {
	size = MM_ALIGN_UP(size);
	size_t tot_size = MM_PAGE_ALIGN(size + MM_METADATA_SIZE);
	void* new = mmap(NULL, tot_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (new == (void*)-1) {
#ifdef MM_DEBUG
		perror("mmap");
#endif
		return 0;
	}

	header_t* header = (header_t*)new;
	header->size = MM_SET_MMAP(MM_CLR_FREE(size));

	return (void*)((uint8_t*)new + MM_HEADER_SIZE);
}

void mm_mmap_free(header_t* header) {
	size_t size = MM_GET_SIZE(header) + MM_METADATA_SIZE;

	if (munmap((void*)header, size) == -1) {
#ifdef MM_DEBUG
		perror("mmap");
#endif
	}
}
