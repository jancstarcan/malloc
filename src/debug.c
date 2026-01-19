#include "interface.h"

#include <assert.h>

void debug_test(void) {
	heap_check();
	free_check();
}

void heap_check(void) {
	Header* cur = (Header*)heap_start;
	while ((void*)cur < heap_end) {
		size_t size = GET_SIZE(cur);
		size_t footer_value = *FOOTER(cur);

		assert(size % ALIGNMENT == 0);
		assert(footer_value == size);

		cur = NEXT_HEADER(cur);
	}
}

void free_check(void) {
	Header* cur = free_list;
	while (cur) {
		assert(IS_FREE(cur));
		cur = cur->next;
	}
}

void canary_check(void) {}

void free_corruption_check(void) {}
