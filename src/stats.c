#include "interface.h"

#include <stdio.h>

static void format_size(char* buf, size_t bytes) {
	const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
	int u = 0;
	double s = (double)bytes;

	while (s >= 1024 && u < 4) {
		s /= 1024;
		u += 1;
	}

	snprintf(buf, 64, "%.2f%s", s, units[u]);
}

#ifdef MM_DEBUG
size_t heap_bytes, mmap_bytes, heap_allocs, mmap_allocs;
inline void mm_add_alloced(size_t n, _Bool mmap) {
	if (mmap) {
		mmap_bytes += n;
		mmap_allocs++;
	} else {
		heap_bytes += n;
		heap_allocs++;
	}
}

void mm_print_alloced(void) {
	char buf[64];
	header_t* h = mm_heap_start;
	footer_t* f;
	size_t s;

	printf("Heap:\n");
	while ((void*)h < mm_heap_end) {
		if (MM_IS_FREE(h)) {
			h = MM_NEXT_HEADER(h);
			continue;
		}

		s = MM_GET_SIZE(h);
		f = MM_FOOTER(h);

		if (s != f->size) {
			fprintf(stderr, "MM_FOOTER MISMATCH at %p\n", (void*)h);
			MM_ABORT();
		}

		format_size(buf, s);
		printf("%s | %p | size=%s\n", MM_IS_FREE(h) ? "FREE" : "USED", (void*)h, buf);

		h = MM_NEXT_HEADER(h);
	}
}

void mm_print_free(void) {
	header_t* prev = NULL;
	int steps = 0;
	char buf[64];

	for (size_t i = 0; i < MM_BIN_COUNT; i++) {
		header_t* cur = mm_free_lists[i];

		printf("Free List %zu:\n", i);
		while (cur) {
			steps++;
			format_size(buf, MM_GET_SIZE(cur));
			printf("prev=0x%p | size=%s | next=0x%p\n", (void*)prev, buf, (void*)MM_GET_NEXT(cur));

			if (steps >= 10000) {
				fprintf(stderr, "Over 10000 entries in the free list, potential cycle\n");
			}

			prev = cur;
			cur = MM_GET_NEXT(cur);
		}
	}
}

void mm_print_stats(void) {
	printf("Stats:\n\n");
	char buf[64];

	size_t tot_bytes = heap_bytes + mmap_bytes;
	size_t tot_allocs = heap_allocs + mmap_allocs;

	format_size(buf, mm_heap_size);
	printf("Heap size is %s\n", buf);
	format_size(buf, tot_bytes);
	printf("%zu blocks were allocated %s\n", tot_allocs, buf);
	format_size(buf, heap_bytes);
	printf("%zu in the heap %s\n", heap_allocs, buf);
	format_size(buf, mmap_bytes);
	printf("%zu with mmap %s\n", mmap_allocs, buf);
}
#else
inline void mm_add_alloced(size_t n, _Bool mmap) {}
void mm_print_alloced(void) {}
void mm_print_free(void) {}
void mm_print_stats(void) {}
#endif
