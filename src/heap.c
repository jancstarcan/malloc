#include "interface.h"

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

void* heap_start;
void* heap_end;
size_t heap_size = 0;
_Bool heap_initialized = 0;

#ifdef MM_DEBUG
_Static_assert(!(MM_ALIGNMENT & 0x1), "MM_ALIGNMENT must be even");
#endif

// Allocates the first INITIAL_HEAP_SIZE bytes
_Bool init_heap(void) {
	heap_size = INITIAL_HEAP_SIZE;
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

	heap_start = sbrk(0);

	if (heap_start == (void*)-1 || sbrk(heap_size) == (void*)-1) {
#ifdef MM_DEBUG
		perror("sbrk");
#endif
		return 0;
	}

	heap_end = sbrk(0);

	size_t payload = heap_size - MM_HEADER_SIZE - MM_CANARY_SIZE - MM_FOOTER_SIZE;

	header_t* h = (header_t*)heap_start;
	h->size = MM_SET_XFREE(payload);
	MM_SET_NEXT(h, NULL);
	MM_FOOTER(h)->size = payload;
	add_to_free(h);

	poison_free(MM_PAYLOAD(h));
	heap_initialized = 1;

	return 1;
}

// Doubles heap size
_Bool grow_heap(void) {
	if (heap_size > SIZE_MAX / 2)
		return 0;

	void* old_end = heap_end;
	if (sbrk(heap_size) == (void*)-1) {
#ifdef MM_DEBUG
		perror("sbrk");
#endif
		return 0;
	}

	heap_end = sbrk(0);

	footer_t* last_footer = (footer_t*)((uint8_t*)old_end - MM_FOOTER_SIZE);
	header_t* last_header = (header_t*)((uint8_t*)last_footer - MM_CANARY_SIZE - last_footer->size - MM_HEADER_SIZE);
	void* payload;

	// If the last block is free it gets extended
	// Otherwise a new one is created
	if (MM_IS_FREE(last_header)) {
		remove_free(last_header);
		size_t new_size = heap_size + MM_GET_SIZE(last_header);
		last_header->size = MM_SET_XFREE(new_size);
		MM_FOOTER(last_header)->size = new_size;
		add_to_free(last_header);
		payload = MM_PAYLOAD(last_header);
	} else {
		header_t* new_header = (header_t*)old_end;
		size_t new_size = heap_size - MM_HEADER_SIZE - MM_CANARY_SIZE - MM_FOOTER_SIZE;
		new_header->size = MM_SET_XFREE(new_size);
		MM_FOOTER(new_header)->size = new_size;
		payload = MM_PAYLOAD(new_header);
		add_to_free(new_header);
	}

	heap_size *= 2;
	poison_free(payload);

	return 1;
}

// Allocates the requested size directly with mmap
// should only be used on big chunks
void* mmap_alloc(size_t size) {
	size = MM_ALIGN_UP(size);
	size_t tot_size = size + MM_HEADER_SIZE + MM_CANARY_SIZE;
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

void mmap_free(header_t* header) {
	size_t size = MM_HEADER_SIZE + MM_GET_SIZE(header) + MM_CANARY_SIZE;

	if (munmap((void*)header, size) == -1) {
#ifdef MM_DEBUG
		perror("mmap");
#endif
	}
}
