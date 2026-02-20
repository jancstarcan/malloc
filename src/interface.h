/*
 * Internal allocator implementation details.
 * Do not include outside allocator source files.
 */

#ifndef MM_PRIVATE_HEADER
#define MM_PRIVATE_HEADER

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/*
 * INTERNAL ALLOCATOR LAYOUT
 *
 * Header->size encodes:
 *   - payload size (aligned)
 *   - MM_MMAP_BIT (is allocated with mmap)
 *   - MM_FREE_BIT (is free)
 *
 * Free list:
 *   - Multiple segregated lists
 *   - MM_BIN_COUNT lists, up to 63
 *   - Singly-linked
 *   - First-fit
 *   - In debug mode the next and previous pointers are part of header_t
 *   - In release mode they're stored in the payload
 *   - That is to try to prevent a use-after-free from corrupting the free list
 *   - Either way GET/SET_PREV/NEXT is the correct way to access them
 *
 * Block layout:
 *
 *   Normal:
 *     [ Header | Payload ]
 *
 *   DEBUG:
 *     [ Header | Payload | Canary ]
 *
 * Invariants:
 *   - Free blocks must be in the free list
 *   - The free list must not include duplicates
 *   - All blocks are MM_ALIGNMENT-aligned
 *   - header->prev must always be correct
 */

#define MM_BIN_COUNT 32
#define MM_BIN_BASE MM_ALIGNMENT

#if MM_BIN_COUNT <= 8
typedef uint8_t free_map_t;
#elif MM_BIN_COUNT <= 16
typedef uint16_t free_map_t;
#elif MM_BIN_COUNT <= 32
typedef uint32_t free_map_t;
#elif MM_BIN_COUNT < 64
typedef uint64_t free_map_t;
#else
#error "Too many bins for bitmap"
#endif

#define MM_SLAB_COUNT 32

typedef struct header header_t;
typedef struct free_list free_list_t;
typedef struct mapping mapping_t;
typedef struct region region_t;
typedef struct arena arena_t;
typedef struct slab slab_t;

#ifdef MM_DEBUG
struct header {
	size_t size;
	header_t* prev;
	header_t* prev_free;
	header_t* next_free;
};
#define MM_PAYLOAD_PTRS 0
#else
struct header {
	size_t size;
	header_t* prev;
};
#define MM_PAYLOAD_PTRS 2
#endif

struct free_list {
	header_t* start[MM_BIN_COUNT];
	free_map_t bitmap;
};

struct region {
	region_t* next;
	arena_t* arena;
	_Bool is_slab;
	_Bool is_free;
};

struct slab {
	uint64_t bitmap[32];
	uint16_t block_size;
	uint16_t free_count;
};

struct arena {
	free_list_t free;
	size_t map_count;
	region_t* reg;
	region_t* tail;

	slab_t slabs[MM_SLAB_COUNT];
	size_t slab_map_count;
	mapping_t* slab_map;
	mapping_t* slab_tail;
};

extern arena_t mm_arena;
extern _Bool mm_arena_initialized;

#define MM_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MM_MIN(a, b) ((a) < (b) ? (a) : (b))
#define MM_ALIGNMENT alignof(max_align_t)
#define MM_ALIGN_UP(x) (((x) + MM_ALIGNMENT - 1) & ~(MM_ALIGNMENT - 1))

#define MMAP_THRESHOLD MM_KB(128)

#ifdef MM_DEBUG
#define MM_CANARY_BYTE 0xCC
#define MM_POISON_FREE_BYTE 0xDD
#define MM_POISON_ALLOC_BYTE 0xAA
#define MM_CANARY_SIZE MM_ALIGN_UP(sizeof(size_t))
#define MM_ENABLE_CANARIES
#define MM_ENABLE_POISONING
#define MM_SAFE_ADD
#define MM_SAFE_REMOVE
#else
#define MM_CANARY_SIZE 0
#endif

#define MM_KB(x) ((size_t)(x) * 1024)
#define MM_REG_SIZE MM_KB(256)
#define MM_REG_CAP MM_KB(4096)
#define MM_REG_METADATA MM_ALIGN_UP(sizeof(region_t))
#define MM_REG_FREE (MM_REG_SIZE - MM_REG_METADATA)
#define MM_HEADER_SIZE MM_ALIGN_UP(sizeof(header_t))
#define MM_METADATA_SIZE (MM_HEADER_SIZE + MM_CANARY_SIZE)

#define MM_MIN_PAYLOAD (MM_ALIGN_UP(MM_PAYLOAD_PTRS * sizeof(void*)))
#define MM_MIN_SPLIT (2 * MM_ALIGNMENT)
#define MM_MIN_BLOCK_SPLIT (MM_MIN_SPLIT + MM_METADATA_SIZE)

#define MM_FREE_BIT 0x1
#define MM_MMAP_BIT 0x2

#define MM_REG_MASK ~((size_t)MM_REG_SIZE - 1)
#define MM_FLAG_MASK ((size_t)MM_ALIGNMENT - 1)
#define MM_SIZE_MASK (~MM_FLAG_MASK)
#define MM_FREE_MASK (~MM_FREE_BIT)
#define MM_MMAP_MASK (~MM_MMAP_BIT)

// #define MM_BIN_BIT(i) ((free_map_t)1 << (i))
static inline free_map_t mm_bin_bit(size_t i) { return (free_map_t)1 << (i); }

static inline void* mm_reg_align(void* ptr) {
	return (void*)(((uintptr_t)ptr + MM_REG_SIZE - 1) & ~((uintptr_t)MM_REG_SIZE - 1));
}

/*
 * MM_GET_SIZE(b): extracts payload size from header
 * MM_CLR_FLAGS(s): clears all flags/returns raw size
 */

// #define MM_GET_SIZE(b) ((b)->size & MM_SIZE_MASK)
// #define MM_CLR_FLAGS(s) ((s) & MM_SIZE_MASK)
// #define MM_IS_FREE(b) (((b)->size & MM_FREE_BIT) != 0)
// #define MM_IS_MMAP(b) (((b)->size & MM_MMAP_BIT) != 0)
static inline size_t mm_get_size(header_t* h) { return h->size & MM_SIZE_MASK; }
static inline size_t mm_clr_flags(size_t s) { return s & MM_SIZE_MASK; }
static inline _Bool mm_is_free(header_t* h) { return (h->size & MM_FREE_BIT) != 0; }
static inline _Bool mm_is_mmap(header_t* h) { return (h->size & MM_MMAP_BIT) != 0; }
/*
 * SET_ FREE/MMAP set bit
 * SET_X FREE/MMAP clear all bits then set
 * CLR_ FREE/MMAP remove bit
 */

// #define MM_SET_FREE(s) ((s) | MM_FREE_BIT)
// #define MM_SET_MMAP(s) ((s) | MM_MMAP_BIT)
// #define MM_CLR_FREE(s) ((s) & MM_FREE_MASK)
// #define MM_CLR_MMAP(s) ((s) & MM_MMAP_MASK)
static inline size_t mm_set_free(size_t s) { return s | MM_FREE_BIT; }
static inline size_t mm_set_mmap(size_t s) { return s | MM_MMAP_BIT; }
static inline size_t mm_clr_free(size_t s) { return s & MM_FREE_MASK; }
static inline size_t mm_clr_mmap(size_t s) { return s & MM_MMAP_MASK; }

// #define MM_SET_XFREE(s) (MM_SET_FREE(MM_CLR_FLAGS((s))))
// #define MM_SET_XMMAP(s) (MM_SET_MMAP(MM_CLR_FLAGS((s))))
static inline size_t mm_set_xfree(size_t s) { return mm_set_free(mm_clr_flags(s)); }
static inline size_t mm_set_xmmap(size_t s) { return mm_set_mmap(mm_clr_flags(s)); }

/*
 * Inline functions invariants:
 *   - Pointers should all be valid and not NULL
 *   - Undefined behavior if heap is corrupted
 *   - MM_PREV/NEXT_HEADER can return pointers past a region's boundary
 *
 * These functions will gladly read junk if they aren't used correctly,
 * all responsibility falls on the caller
 */

#ifdef MM_DEBUG
static inline header_t** mm_get_next_ptr(header_t* h) { return &h->next_free; }
static inline void mm_set_next(header_t* h, header_t* next) { h->next_free = next; }
static inline header_t** mm_get_prev_ptr(header_t* h) { return &h->prev_free; }
static inline void mm_set_prev(header_t* h, header_t* prev) { h->prev_free = prev; }
#else
static inline header_t** mm_get_next_ptr(header_t* h) { return (header_t**)((uint8_t*)h + MM_HEADER_SIZE); }
static inline void mm_set_next(header_t* h, header_t* next) { *((header_t**)((uint8_t*)h + MM_HEADER_SIZE)) = next; }
static inline header_t** mm_get_prev_ptr(header_t* h) {
	return (header_t**)((uint8_t*)h + MM_HEADER_SIZE + sizeof(void*));
}
static inline void mm_set_prev(header_t* h, header_t* prev) {
	*((header_t**)((uint8_t*)h + MM_HEADER_SIZE + sizeof(void*))) = prev;
}
#endif
static inline header_t* mm_get_next(header_t* h) { return *mm_get_next_ptr(h); }
static inline header_t* mm_get_prev(header_t* h) { return *mm_get_prev_ptr(h); }

static inline region_t* mm_get_reg(void* ptr) { return (region_t*)((uintptr_t)ptr & MM_REG_MASK); }
static inline void* mm_get_reg_end(region_t* reg) { return (void*)((uint8_t*)reg + MM_REG_SIZE); }
static inline void* mm_get_reg_start(region_t* reg) { return (void*)((uint8_t*)reg + MM_REG_METADATA); }

static inline header_t* mm_header(void* payload) { return (header_t*)((uint8_t*)payload - MM_HEADER_SIZE); }
static inline size_t* mm_canary(header_t* h) { return (size_t*)((uint8_t*)h + MM_HEADER_SIZE + mm_get_size(h)); }
static inline void* mm_payload(header_t* h) { return (void*)((uint8_t*)h + MM_HEADER_SIZE); }
static inline header_t* mm_next_header(header_t* h) {
	return (header_t*)((uint8_t*)h + mm_get_size(h) + MM_METADATA_SIZE);
}
static inline void mm_link_next_header(header_t* h) { mm_next_header(h)->prev = h; }

#define MM_ABORT() __builtin_trap()

// debug.c
void mm_debug_test(void);
void mm_write_canary(header_t* h);
void mm_check_canary(header_t* h);
void mm_poison_free(void* p);
void mm_poison_alloc(void* p);
void mm_poison_free_area(void* p, size_t s);
void mm_poison_alloc_area(void* p, size_t s);
void mm_reg_check(region_t* reg);
void mm_free_check(free_list_t* free);

#ifdef MM_DEBUG
static inline void mm_run_checks() { mm_debug_test(); }
#else
static inline void mm_run_checks() {}
#endif

// arena.c
_Bool mm_init_arena(void);
_Bool mm_grow_arena(arena_t* arena);
void* mm_mmap_alloc(size_t size);
void mm_mmap_free(header_t* header);

// free_list.c
size_t mm_idx_from_size(size_t s);
size_t mm_size_from_idx(size_t i);
void mm_add_to_free(header_t* h);
_Bool mm_remove_free(header_t* h);
header_t* mm_find_fit(size_t size);

// block.c
void mm_coalesce_prev(header_t** header_ptr);
void mm_coalesce_next(header_t* header);
void mm_shrink_block(header_t* header, size_t size, _Bool is_free);
_Bool mm_grow_block(header_t* header, size_t size, _Bool is_free);
void* mm_malloc_block(size_t size);

// mem.c
void* malloc(size_t size);
void* realloc(void* ptr, size_t size);
void* calloc(size_t size, size_t n);
void free(void* ptr);

// stats.c
void mm_add_alloced(size_t n, _Bool mmap);
void mm_print_alloced(void);
void mm_print_free(void);
void mm_print_stats(void);

// slabs.c

// asserts
_Static_assert((MM_REG_SIZE & (MM_REG_SIZE - 1)) == 0, "MM_REG_SIZE must be a power of two");

#endif
