#include "mem.h"
#include <assert.h>

int rmain(void) {
	void* ptr = malloc(32);
	void* new_ptr = realloc(ptr, 64);
	assert(ptr == new_ptr);
	return 0;
}
