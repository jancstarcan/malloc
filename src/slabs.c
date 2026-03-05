#include "interface.h"

static inline slab_t* alloc_slab(size_t size, arena_t* arena) {
    slab_region_t* tail = (slab_region_t*)arena->slab_tail;
	slab_region_t* curr = (slab_region_t*)arena->slab_reg;

    while (curr && !curr->slabs_bitmap) {
        curr = (slab_region_t*)((region_t*)curr)->next;
    }

    if (!curr) {
        if (!mm_grow_slabs(arena)) {
            return NULL;
        }
        curr = (slab_region_t*)tail->base.next;
    }

    int idx = __builtin_ctzll(curr->slabs_bitmap);
    curr->slabs_bitmap &= ~((uint64_t)1 << idx);
    slab_t* slab = &curr->slabs_metadata[idx];

    slab->block_size = size;
    slab->state = SLAB_NONE;

    size_t bitmap_count = mm_get_bitmap_count(size);
    for (size_t i = 0; i < bitmap_count; i++) {
        slab->bitmap[i] = (uint64_t)-1;
    }

    slab->bitmap_bitmap = (uint64_t)-1 & (((uint64_t)1 << bitmap_count) - 1);

    return slab;
}

static inline slab_t* find_slab(size_t size, arena_t* arena) {
    size_t size_class = size / MM_SLAB_BASE;
    slab_list_t* list = &arena->slab_lists[size_class];

    slab_t* slab;
    if (list->part) {
        slab = list->part;
    } else if (list->free) {
        slab = list->free;
        list->free = NULL;
    } else {
        slab = alloc_slab(size, arena);
    }

    return slab;
}

static inline slab_t** get_slab_list_set(slab_list_t* list, slab_state_t state) {
    switch (state) {
    case SLAB_FULL:
        return &list->full;
    case SLAB_PART:
        return &list->part;
    case SLAB_FREE:
        return &list->free;
    default:
        return NULL;
    }
}

static inline void remove_slab(slab_t* slab) {
    slab_t* prev = slab->prev;
    slab_t* next = slab->next;

    if (!prev) {
        slab_list_t* list = mm_get_slab_list(slab);
        slab_t** start = get_slab_list_set(list, slab->state);

        *start = next;
        if (next) {
            next->prev = NULL;
        }
    } else {
        prev->next = next;
        if (next) {
            next->prev = prev;
        }
    }

    slab->state = SLAB_NONE;
    slab->prev = NULL;
    slab->next = NULL;
}

static inline void move_slab(slab_t* slab, slab_state_t new_state) {
    if (slab->state != SLAB_NONE) {
        remove_slab(slab);
    }

    slab_list_t* list = mm_get_slab_list(slab);
    slab_t** start = get_slab_list_set(slab, new_state);
}

void* mm_slab_malloc(size_t size, arena_t* arena) {
    slab_t* slab = find_slab(size, arena);
    size_t bitmap_idx = __builtin_ctzll(slab->bitmap_bitmap);
    uint64_t bitmap = slab->bitmap[bitmap_idx];
    size_t block_idx = __builtin_ctzll(bitmap);

    void* block = mm_get_slab_block(slab, block_idx);
    slab->bitmap[bitmap_idx] &= ~MM_U64b(block_idx);

    if (slab->bitmap[bitmap_idx] == 0) {
        slab->bitmap_bitmap &= ~MM_U64b(bitmap_idx);
        if (slab->bitmap_bitmap == 0) {
            slab->state = SLAB_FULL;
        }
    }

    return block;
}

void* mm_slab_malloc(void* ptr) {}
