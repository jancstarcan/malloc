#include "interface.h"

#include <assert.h>
#include <string.h>

void debug_test(void) {
	heap_check();
	free_check();
}

#ifdef ENABLE_CANARIES
inline void write_canary(Header* h) {
	uint8_t* c = CANARY(h);
	memset(c, CANARY_BYTE, CANARY_SIZE);
}
inline void check_canary(Header* h) {
	uint8_t* c = CANARY(h);
	for (size_t i = 0; i < CANARY_SIZE; i++)
		assert(*c++ == CANARY_BYTE);
}
#else
inline void write_canary(Header* h) {}
inline void check_canary(Header* h) {}
#endif

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
