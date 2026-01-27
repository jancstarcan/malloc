#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
size_t heap_bytes, mmap_bytes, heap_allocs, mmap_allocs;
static inline void add_alloced(size_t n, _Bool mmap) {
	if (mmap) {
		mmap_bytes += n;
		mmap_allocs++;
	} else {
		heap_bytes += n;
		heap_allocs++;
	}
}
#else
static inline void add_alloced(size_t n, _Bool mmap) {}
#endif

void* malloc(size_t size) {
	if (size == 0)
		return NULL;

	size = ALIGN_UP(size);

	if (size >= MMAP_THRESHOLD) {
		void* p = mmap_alloc(size);
		write_canary(HEADER(p));
		poison_alloc(p);

		add_alloced(size, 1);

		return p;
	}

	void* p = malloc_block(size);
	write_canary(HEADER(p));
	poison_alloc(p);

	add_alloced(size, 0);

	RUN_CHECKS();
	return p;
}

void free(void* ptr) {
	if (!ptr)
		return;

	header_t* header = HEADER(ptr);
	check_canary(header);

	// Debug mode check for pointer validity
#ifdef DEBUG
	if (((void*)header < heap_start || (void*)header >= heap_end) &&
		!IS_MMAP(header)) {
		fprintf(stderr, "Ptr is not in the accepted range\n");
		fflush(stderr);
		abort();
	}
#endif

	poison_free(ptr);

	if (IS_MMAP(header)) {
		mmap_free(header);
		return;
	}

	// Double free check
	if (IS_FREE(header)) {
#ifdef DEBUG
		fprintf(stderr, "Double free detected\n");
		fflush(stderr);
		abort();
#else
		return;
#endif
	}

	header->size = SET_FREE(header->size);
	coalesce_prev(&header);
	coalesce_next(header);
	add_to_free(header);

	RUN_CHECKS();
}

void* realloc(void* ptr, size_t size) {
	if (size == 0) {
		free(ptr);
		return NULL;
	}

	// Realloc acts as malloc if no pointer is provided
	if (!ptr)
		return malloc(size);

	header_t* header = HEADER(ptr);
	size_t old_size = GET_SIZE(header);
	size = ALIGN_UP(size);

	if (size == old_size) {
		// No change in size
		return ptr;
	} else if (size < old_size) {
		shrink_block(header, size);

		write_canary(header);
		RUN_CHECKS();
		return ptr;
	}

	if (try_grow_in_place(header, size)) {
		write_canary(header);
		add_alloced(size - old_size, 0);
		RUN_CHECKS();
		return ptr;
	}

	// In case the next block isn't big enough or isn't free,
	// malloc_block is called
	void* new_ptr = malloc_block(size);
	if (!new_ptr)
		return NULL;

	poison_alloc(new_ptr);

	memcpy(new_ptr, ptr, old_size);
	free(ptr);

	add_alloced(size, 0);

	write_canary(HEADER(new_ptr));
	RUN_CHECKS();

	return new_ptr;
}

// This is just malloc with memset(0) and a bounds check
void* calloc(size_t size, size_t n) {
	if (size == 0 || n == 0 || size > SIZE_MAX / n)
		return NULL;

	size_t tot_size = ALIGN_UP(size * n);
	if (tot_size < size * n)
		return NULL;

	void* ptr;

	if (tot_size >= MMAP_THRESHOLD) {
		ptr = mmap_alloc(tot_size);
		add_alloced(tot_size, 1);
	} else {
		ptr = malloc_block(tot_size);
		add_alloced(tot_size, 0);
	}

	if (!ptr)
		return NULL;

	memset(ptr, 0, tot_size);
	write_canary(HEADER(ptr));
	RUN_CHECKS();

	return ptr;
}

#ifdef DEBUG
void dump_heap() {
	header_t* h = heap_start;
	footer_t* f;
	size_t s;

	printf("Heap:\n");
	while ((void*)h < heap_end) {
		s = GET_SIZE(h);
		f = FOOTER(h);

		if (s < MIN_BLOCK_SIZE || s > heap_size) {
			fprintf(stderr, "CORRUPTED BLOCK at %p\n", (void*)h);
			abort();
		}

		if (s != f->size) {
			fprintf(stderr, "FOOTER MISMATCH at %p\n", (void*)h);
			abort();
		}

		printf("%s | %p | size=%zu\n", IS_FREE(h) ? "FREE" : "USED", (void*)h,
			   s);

		h = NEXT_HEADER(h);
	}
}

void dump_free_list(void) {
	header_t* prev = NULL;
	int steps = 0;

	for (size_t i = 0; i < BIN_COUNT; i++) {
		header_t* cur = free_lists[i];

		printf("Free List %zu:\n", i);
		while (cur) {
			steps++;
			printf("prev=0x%p | size=%zu | next=0x%p\n", (void*)prev,
				   GET_SIZE(cur), (void*)GET_NEXT(cur));

			if (steps >= 10000) {
				fprintf(
					stderr,
					"Over 10000 entries in the free list, potential cycle\n");
			}

			prev = cur;
			cur = GET_NEXT(cur);
		}
	}
}

void format_size(char* buf, size_t bytes) {
	const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
	int u = 0;
	double s = (double)bytes;

	while (s >= 1024 && u < 4) {
		s /= 1024;
		u += 1;
	}

	snprintf(buf, 64, "%.2f%s", s, units[u]);
}

void print_stats(void) {
	printf("Stats:\n\n");
	char buf[64];

	size_t tot_bytes = heap_bytes + mmap_bytes;
	size_t tot_allocs = heap_allocs + mmap_allocs;

	format_size(buf, tot_bytes);
	printf("%zu blocks were allocated %s\n", tot_allocs, buf);
	format_size(buf, heap_bytes);
	printf("%zu in the heap %s\n", heap_allocs, buf);
	format_size(buf, mmap_bytes);
	printf("%zu with mmap %s\n", mmap_allocs, buf);
}
#else
void dump_heap() {}
void dump_free_list() {}
void print_stats() {}
#endif
