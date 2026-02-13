#include "interface.h"

header_t* free_lists[MM_BIN_COUNT] = {0};
bins_map_t bins_map = 0;

size_t idx_from_size(size_t s) {
	size_t bits = sizeof(size_t) * 8;
	size_t units = s / MM_BIN_BASE;
	size_t i = bits - 1 - __builtin_clzll(units);

	if (i >= MM_BIN_COUNT)
		return MM_BIN_COUNT - 1;

	return i;
}

size_t size_from_idx(size_t i) {
	if (i >= MM_BIN_COUNT)
		i = MM_BIN_COUNT - 1;
	return MM_BIN_BASE << i;
}

void add_to_free(header_t* h) {
	size_t s = MM_GET_SIZE(h);
	size_t i = idx_from_size(s);
	MM_SET_NEXT(h, free_lists[i]);
	free_lists[i] = h;
	bins_map |= MM_BIN_BIT(i);
}

_Bool remove_free(header_t* h) {
	size_t s = MM_GET_SIZE(h);
	size_t i = idx_from_size(s);
	header_t** cur = &free_lists[i];

	while (*cur && *cur != h)
		cur = &MM_GET_NEXT(*cur);

	if (!*cur)
		return 0;

	*cur = MM_GET_NEXT(*cur);

	if (!free_lists[i])
		bins_map &= ~MM_BIN_BIT(i);

	return 1;
}

header_t* find_fit(size_t s) {
	size_t i = idx_from_size(s);
	header_t** cur;

	while (i < MM_BIN_COUNT) {
		bins_map_t mask = bins_map & ((bins_map_t)-1 << i);
		if (!mask)
			return NULL;
		i = __builtin_ctz(mask);

		cur = &free_lists[i];

		while (*cur) {
			if (MM_GET_SIZE(*cur) >= s)
				break;

			cur = &MM_GET_NEXT(*cur);
		}

		if (*cur)
			break;

		i++;
	}

	if (!*cur)
		return NULL;

	header_t* ret = *cur;
	*cur = MM_GET_NEXT(*cur);

	if (!free_lists[i])
		bins_map &= ~MM_BIN_BIT(i);

	return ret;
}
