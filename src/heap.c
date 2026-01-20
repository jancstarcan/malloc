#include "interface.h"

#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

void* heap_start;
void* heap_end;
size_t heap_size = 0;
_Bool heap_initialized = 0;

// Allocates the first INITIAL_HEAP_SIZE bytes
_Bool init_heap() {
	heap_size = INITIAL_HEAP_SIZE;
	heap_start = sbrk(0);
	if (heap_start == (void*)-1 || sbrk(heap_size) == (void*)-1) {
#ifdef DEBUG
		perror("sbrk");
#endif
		return 0;
	}

	heap_end = sbrk(0);

	size_t payload = heap_size - HEADER_SIZE - CANARY_SIZE - FOOTER_SIZE;

	free_list = (Header*)heap_start;
	free_list->size = SET_XFREE(payload);
	free_list->next = NULL;
	*FOOTER(free_list) = payload;

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

	size_t* last_footer = (size_t*)((uint8_t*)old_end - FOOTER_SIZE);
	Header* last_header =
		(Header*)((uint8_t*)last_footer - CANARY_SIZE - *last_footer - HEADER_SIZE);

	// If the last block is free it gets extended
	// Otherwise a new one is created
	if (IS_FREE(last_header)) {
		size_t new_size = heap_size + GET_SIZE(last_header);
		last_header->size = SET_XFREE(new_size);
		*FOOTER(last_header) = new_size;
	} else {
		Header* new_header = (Header*)old_end;
		size_t new_size = heap_size - HEADER_SIZE - CANARY_SIZE - FOOTER_SIZE;
		new_header->size = SET_XFREE(new_size);
		*FOOTER(new_header) = new_size;
		new_header->next = free_list;
		free_list = new_header;
	}

	heap_size *= 2;
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

	Header* header = (Header*)new;
	header->size = SET_MMAP(CLR_FREE(size));

	return (void*)((uint8_t*)new + HEADER_SIZE);
}

void mmap_free(Header* header) {
	size_t size = HEADER_SIZE + GET_SIZE(header) + CANARY_SIZE;

	if (munmap((void*)header, size) == -1) {
#ifdef DEBUG
		perror("mmap");
#endif
	}
}
