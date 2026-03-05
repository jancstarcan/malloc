#include "interface.h"
#include <stdio.h>

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
	region_t* reg = mm_get_reg(h);
	free_list_t* free = &reg->arena->free;
	header_t** lists = free->start;
	free_map_t* bitmap = &free->bitmap;

	size_t s = mm_get_size(h);
	size_t i = mm_idx_from_size(s);
	mm_set_next(h, lists[i]);
	lists[i] = h;
	*bitmap |= mm_bin_bit(i);
}
#else
void mm_add_to_free(header_t* h) {
	region_t* reg = mm_get_reg(h);
	free_list_t* free = &reg->arena->free;
	header_t** lists = free->start;
	free_map_t* bitmap = &free->bitmap;

	size_t s = mm_get_size(h);
	size_t i = mm_idx_from_size(s);
	header_t* old_head = lists[i];
	if (old_head) {
		mm_set_prev(lists[i], h);
	}

	mm_set_next(h, lists[i]);
	mm_set_prev(h, NULL);
	lists[i] = h;
	*bitmap |= mm_bin_bit(i);
}
#endif

#ifdef MM_SAFE_REMOVE
_Bool mm_remove_free(header_t* h) {
	region_t* reg = mm_get_reg(h);
	free_list_t* free = &reg->arena->free;
	header_t** lists = free->start;
	free_map_t* bitmap = &free->bitmap;

	size_t s = mm_get_size(h);
	size_t i = mm_idx_from_size(s);
	header_t** cur = &lists[i];

	while (*cur && *cur != h)
		cur = mm_get_next_ptr(*cur);

	if (!*cur)
		return 0;

	*cur = mm_get_next(*cur);

	if (!lists[i])
		*bitmap &= ~mm_bin_bit(i);

	return 1;
}
#else
_Bool mm_remove_free(header_t* h) {
	region_t* reg = mm_get_reg(h);
	free_list_t* free = &reg->arena->free;
	header_t** lists = free->start;
	free_map_t* bitmap = &free->bitmap;

	header_t* prev = mm_get_prev(h);
	header_t* next = mm_get_next(h);

	if (!prev) {
		size_t i = mm_idx_from_size(mm_get_size(h));
		lists[i] = next;
		if (next) {
			mm_set_prev(next, NULL);
		} else {
			*bitmap &= ~mm_bin_bit(i);
		}
	} else {
		mm_set_next(prev, next);
		if (next) {
			mm_set_prev(next, prev);
		}
	}

	return 1;
}
#endif

header_t* mm_find_fit(size_t s, arena_t* arena) {
	free_list_t* free = &arena->free;
	header_t** lists = free->start;
	free_map_t* bitmap = &free->bitmap;

	size_t i = mm_idx_from_size(s);
	header_t** cur;

	while (i < MM_BIN_COUNT) {
		free_map_t mask = *bitmap & ((free_map_t)-1 << i);
		if (!mask)
			return NULL;
		i = __builtin_ctz(mask);

		cur = &lists[i];

		while (*cur) {
			if (mm_get_size(*cur) >= s)
				break;

			cur = mm_get_next_ptr(*cur);
		}

		if (*cur) {
			break;
		}

		i++;
	}

	if (!*cur) {
		return NULL;
	}

	header_t* ret = *cur;
	mm_remove_free(ret);

	if (!lists[i])
		*bitmap &= ~mm_bin_bit(i);

	return ret;
}
