#include "interface.h"

Header* free_list = NULL;

void add_to_free(Header* header) {
	header->next = free_list;
	free_list = header;
}

_Bool remove_free(Header* header) {
	Header** cur = &free_list;

	while (*cur && *cur != header)
		cur = &(*cur)->next;

	if (!*cur)
		return 0;

	*cur = (*cur)->next;
	return 1;
}

Header* find_fit(size_t size) {
	Header** cur = &free_list;
	while (*cur) {
		if (GET_SIZE(*cur) >= size)
			break;

		cur = &(*cur)->next;
	}

	if (!*cur)
		return NULL;

	Header* ret = *cur;
	*cur = (*cur)->next;
	return ret;
}
