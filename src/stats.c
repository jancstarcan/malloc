#include "interface.h"

#include <stdio.h>

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

#ifdef DEBUG
size_t heap_bytes, mmap_bytes, heap_allocs, mmap_allocs;
inline void add_alloced(size_t n, _Bool mmap) {
	if (mmap) {
		mmap_bytes += n;
		mmap_allocs++;
	} else {
		heap_bytes += n;
		heap_allocs++;
	}
}

void print_alloced() {
	char buf[64];
	header_t* h = heap_start;
	footer_t* f;
	size_t s;

	printf("Heap:\n");
	while ((void*)h < heap_end) {
		s = GET_SIZE(h);
		f = FOOTER(h);

		if (s != f->size) {
			fprintf(stderr, "FOOTER MISMATCH at %p\n", (void*)h);
			abort();
		}

		format_size(buf, s);
		printf("%s | %p | size=%s\n", IS_FREE(h) ? "FREE" : "USED", (void*)h,
			   buf);

		h = NEXT_HEADER(h);
	}
}

void print_free(void) {
	header_t* prev = NULL;
	int steps = 0;
	char buf[64];

	for (size_t i = 0; i < BIN_COUNT; i++) {
		header_t* cur = free_lists[i];

		printf("Free List %zu:\n", i);
		while (cur) {
			steps++;
			format_size(buf, GET_SIZE(cur));
			printf("prev=0x%p | size=%s | next=0x%p\n", (void*)prev,
				   buf, (void*)GET_NEXT(cur));

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

void print_stats(void) {
	printf("Stats:\n\n");
	char buf[64];

	size_t tot_bytes = heap_bytes + mmap_bytes;
	size_t tot_allocs = heap_allocs + mmap_allocs;

	format_size(buf, heap_size);
	printf("Heap size is %s\n", buf);
	format_size(buf, tot_bytes);
	printf("%zu blocks were allocated %s\n", tot_allocs, buf);
	format_size(buf, heap_bytes);
	printf("%zu in the heap %s\n", heap_allocs, buf);
	format_size(buf, mmap_bytes);
	printf("%zu with mmap %s\n", mmap_allocs, buf);
}
#else
inline void add_alloced(size_t n, _Bool mmap) {}
void dump_heap() {}
void dump_free_list() {}
void print_stats() {}
#endif
