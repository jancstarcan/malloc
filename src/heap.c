#include "interface.h"

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

void* heap_start;
void* heap_end;
size_t heap_size = 0;
_Bool heap_initialized = 0;

#ifdef DEBUG
_Static_assert(!(ALIGNMENT & 0x1), "ALIGNMENT must be even");
#endif

// Allocates the first INITIAL_HEAP_SIZE bytes
_Bool init_heap() {
	heap_size = INITIAL_HEAP_SIZE;
	uintptr_t brk = (uintptr_t)sbrk(0);

	if (brk == (uintptr_t)-1) {
#ifdef DEBUG
		perror("sbrk");
#endif
		return 0;
	}

	size_t mis = brk & (ALIGNMENT - 1);
	if (mis) {
		if (sbrk(ALIGNMENT - mis) == (void*)-1) {
#ifdef DEBUG
			perror("sbrk");
#endif
			return 0;
		}
	}

	heap_start = sbrk(0);

	if (heap_start == (void*)-1 || sbrk(heap_size) == (void*)-1) {
#ifdef DEBUG
		perror("sbrk");
#endif
		return 0;
	}

	heap_end = sbrk(0);

	size_t payload = heap_size - HEADER_SIZE - CANARY_SIZE - FOOTER_SIZE;

	free_list = (header_t*)heap_start;
	free_list->size = SET_XFREE(payload);
	SET_NEXT(free_list, NULL);
	FOOTER(free_list)->size = payload;

	poison_free(PAYLOAD(free_list));
	heap_initialized = 1;

	return 1;
}

// Doubles heap size
_Bool grow_heap() {
	if (heap_size > SIZE_MAX / 2)
		return 0;

	void* old_end = heap_end;
	if (sbrk(heap_size) == (void*)-1) {
#ifdef DEBUG
		perror("sbrk");
#endif
		return 0;
	}

	heap_end = sbrk(0);

	footer_t* last_footer = (footer_t*)((uint8_t*)old_end - FOOTER_SIZE);
	header_t* last_header = (header_t*)((uint8_t*)last_footer - CANARY_SIZE -
									last_footer->size - HEADER_SIZE);
	void* payload;

	// If the last block is free it gets extended
	// Otherwise a new one is created
	if (IS_FREE(last_header)) {
		size_t new_size = heap_size + GET_SIZE(last_header);
		last_header->size = SET_XFREE(new_size);
		FOOTER(last_header)->size = new_size;
		payload = PAYLOAD(last_header);
	} else {
		header_t* new_header = (header_t*)old_end;
		size_t new_size = heap_size - HEADER_SIZE - CANARY_SIZE - FOOTER_SIZE;
		new_header->size = SET_XFREE(new_size);
		FOOTER(new_header)->size = new_size;
		SET_NEXT(new_header, free_list);
		free_list = new_header;
		payload = PAYLOAD(new_header);
	}

	heap_size *= 2;
	poison_free(payload);

	return 1;
}

// Allocates the requested size directly with mmap
// should only be used on big chunks
void* mmap_alloc(size_t size) {
	size = ALIGN_UP(size);
	size_t tot_size = size + HEADER_SIZE + CANARY_SIZE;
	void* new = mmap(NULL, tot_size, PROT_WRITE | PROT_READ,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (new == (void*)-1) {
#ifdef DEBUG
		perror("mmap");
#endif
		return 0;
	}

	header_t* header = (header_t*)new;
	header->size = SET_MMAP(CLR_FREE(size));

	return (void*)((uint8_t*)new + HEADER_SIZE);
}

void mmap_free(header_t* header) {
	size_t size = HEADER_SIZE + GET_SIZE(header) + CANARY_SIZE;

	if (munmap((void*)header, size) == -1) {
#ifdef DEBUG
		perror("mmap");
#endif
	}
}
