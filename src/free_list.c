#include "interface.h"
#include <stdio.h>

header_t* mm_free_lists[MM_BIN_COUNT] = {0};
free_map_t mm_free_map = 0;

size_t mm_idx_from_size(size_t s) {
	size_t bits = sizeof(size_t) * 8;
	size_t units = s / MM_BIN_BASE;
	size_t i = bits - 1 - __builtin_clzll(units);

	if (i >= MM_BIN_COUNT)
		return MM_BIN_COUNT - 1;

	return i;
}

size_t mm_size_from_idx(size_t i) {
	if (i >= MM_BIN_COUNT)
		i = MM_BIN_COUNT - 1;
	return MM_BIN_BASE << i;
}

#ifdef MM_SAFE_ADD
void mm_add_to_free(header_t* h) {
	size_t s = MM_GET_SIZE(h);
	size_t i = mm_idx_from_size(s);
	MM_SET_NEXT(h, mm_free_lists[i]);
	mm_free_lists[i] = h;
	mm_free_map |= MM_BIN_BIT(i);
}
#else
void mm_add_to_free(header_t* h) {
	size_t s = MM_GET_SIZE(h);
	size_t i = mm_idx_from_size(s);
	header_t* old_head = mm_free_lists[i];
	if (old_head) {
		MM_SET_PREV(mm_free_lists[i], h);
	}

	MM_SET_NEXT(h, mm_free_lists[i]);
	MM_SET_PREV(h, NULL);
	mm_free_lists[i] = h;
	mm_free_map |= MM_BIN_BIT(i);
}
#endif

#ifdef MM_SAFE_REMOVE
_Bool mm_remove_free(header_t* h) {
	size_t s = MM_GET_SIZE(h);
	size_t i = mm_idx_from_size(s);
	header_t** cur = &mm_free_lists[i];

	while (*cur && *cur != h)
		cur = MM_GET_NEXT_PTR(*cur);

	if (!*cur)
		return 0;

	*cur = MM_GET_NEXT(*cur);

	if (!mm_free_lists[i])
		mm_free_map &= ~MM_BIN_BIT(i);

	return 1;
}
#else
_Bool mm_remove_free(header_t* h) {
	header_t* prev = MM_GET_PREV(h);
	header_t* next = MM_GET_NEXT(h);

	if (!prev) {
		size_t i = mm_idx_from_size(MM_GET_SIZE(h));
		mm_free_lists[i] = next;
		if (next) {
			MM_SET_PREV(next, NULL);
		} else {
			mm_free_map &= ~MM_BIN_BIT(i);
		}
	} else {
		MM_SET_NEXT(prev, next);
		if (next) {
			MM_SET_PREV(next, prev);
		}
	}

	return 1;
}
#endif

header_t* mm_find_fit(size_t s) {
	size_t i = mm_idx_from_size(s);
	header_t** cur;

	while (i < MM_BIN_COUNT) {
		free_map_t mask = mm_free_map & ((free_map_t)-1 << i);
		if (!mask)
			return NULL;
		i = __builtin_ctz(mask);

		cur = &mm_free_lists[i];

		while (*cur) {
			if (MM_GET_SIZE(*cur) >= s)
				break;

			cur = MM_GET_NEXT_PTR(*cur);
		}

		if (*cur)
			break;

		i++;
	}

	if (!*cur)
		return NULL;

	header_t* ret = *cur;
	mm_remove_free(ret);

	if (!mm_free_lists[i])
		mm_free_map &= ~MM_BIN_BIT(i);

	return ret;
}
