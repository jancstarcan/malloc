#include "interface.h"

header_t* free_list = NULL;

void add_to_free(header_t* header) {
	SET_NEXT(header, free_list);
	free_list = header;
}

_Bool remove_free(header_t* header) {
	header_t** cur = &free_list;

	while (*cur && *cur != header)
		*cur = GET_NEXT(*cur);

	if (!*cur)
		return 0;

	*cur = GET_NEXT(*cur);
	return 1;
}

header_t* find_fit(size_t size) {
	header_t** cur = &free_list;

	while (*cur) {
		if (GET_SIZE(*cur) >= size)
			break;

		*cur = GET_NEXT(*cur);
	}

	if (!*cur)
		return NULL;

	header_t* ret = *cur;
	*cur = GET_NEXT(*cur);
	return ret;
}
