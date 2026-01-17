#include "interface.h"

#include <assert.h>

// Heap validation, only in debug mode
void heap_check(void) {
	Header* cur = (Header*)heap_start;
	while ((void*)cur < heap_end) {
		size_t size = GET_SIZE(cur);
		size_t footer_value = *FOOTER(cur);

		// fprintf(stderr, "Block at %p: size = %zu, footer = %zu\n",
		// (void*)cur, size, footer_value);

		assert(size % ALIGNMENT == 0);
		assert(footer_value == size);

		cur = NEXT_HEADER(cur);
	}
}
