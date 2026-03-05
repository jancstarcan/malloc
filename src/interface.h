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
 *
 * Slabs:
 *   - For small allocations, under MM_SLAB_THRESHOLD, slab allocation is used.
 *   - Slabs are returned by the "slab allocator", which uses the same type of regions as normal allocations,
 *     so any pointer can be masked to obtain the region, and then again to get to the slab.
 *   - Only one free slab should be kept in slab_list_t.free, all others should be returned to the slab allocator.
 *
 * Slab Layout:
 *
 *   [ region metadata | slabs bitmap | slabs metadata [MM_SLABS_IN_REG - 1] ]
 *	 [ slab 1 ]
 *	 [ slab 2 ]
 *	 ...
 *
 * Notes:
 *   - The first would-be slab in every region stores all of the metadata, for both the region and slabs
 *   - All slabs are MM_SLAB_SIZE aligned to allow for slab lookup via masking
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

#define MM_KB(x) ((size_t)(x) * 1024)
#define MM_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MM_MIN(a, b) ((a) < (b) ? (a) : (b))

#define MM_ALIGNMENT MM_MAX(16, alignof(max_align_t))
#define MM_ALIGN_UP(x) (((x) + MM_ALIGNMENT - 1) & ~(MM_ALIGNMENT - 1))
#define MM_SLAB_ALIGN_UP(x) (((x) + MM_SLAB_BASE - 1) & ~(MM_SLAB_BASE - 1))

#define MM_SLAB_SIZE MM_KB(32)
#define MM_REG_SIZE MM_KB(256)

#define MM_SLAB_BASE MM_ALIGNMENT
#define MM_SLAB_SIZE_CLASSES 32
#define MM_SLAB_THRESHOLD (MM_SLAB_BASE * MM_SLAB_SIZE_CLASSES)
#define MM_SLABS_IN_REG (MM_REG_SIZE / MM_SLAB_SIZE)
#define MM_SLAB_BITMAP_COUNT ((MM_SLAB_SIZE / MM_SLAB_BASE) / 64)

#define MM_SLAB_BITMAP(b) (((uint64_t)b) & (((uint64_t)MM_SLABS_IN_REG - 1)))
#define MM_SLAB_BITMAP_BITMAP(b) (((uint64_t)b) & (((uint64_t)MM_SLAB_BITMAP_COUNT - 1)))
#define MM_U64b(n) ((uint64_t)1 << n)

typedef struct header header_t;
typedef struct free_list free_list_t;
typedef struct mapping mapping_t;
typedef struct region region_t;
typedef struct arena arena_t;
typedef struct slab_list slab_list_t;
typedef struct slab slab_t;
typedef struct slab_region slab_region_t;

struct header {
	size_t size;
	header_t* prev;
};
#define MM_PAYLOAD_PTRS 2

struct free_list {
	header_t* start[MM_BIN_COUNT];
	free_map_t bitmap;
};

struct region {
	region_t* next;
	arena_t* arena;
	_Bool is_slab;
};

typedef enum {
	SLAB_NONE,
	SLAB_FULL,
	SLAB_PART,
	SLAB_FREE,
} slab_state_t;
struct slab {
	slab_t* prev;
	slab_t* next;
	void* start;
	uint64_t bitmap[MM_SLAB_BITMAP_COUNT];
	uint64_t bitmap_bitmap;
	slab_state_t state;
	uint16_t block_size;
};

struct slab_region {
	region_t base;
	uint64_t slabs_bitmap;
	slab_t slabs_metadata[MM_SLABS_IN_REG - 1];
};

struct slab_list {
	slab_t* full;
	slab_t* part;
	slab_t* free;
};

struct arena {
	free_list_t free;
	region_t* reg;
	region_t* tail;
	size_t map_count;

	slab_list_t slab_lists[MM_SLAB_SIZE_CLASSES];
	region_t* slab_reg;
	region_t* slab_tail;
	size_t slab_map_count;
};

extern arena_t mm_arena;
extern _Bool mm_arena_initialized;

#define MMAP_THRESHOLD MM_KB(128)

#define MM_REG_CAP MM_KB(4096)
#define MM_REG_METADATA MM_ALIGN_UP(sizeof(region_t))
#define MM_REG_FREE (MM_REG_SIZE - MM_REG_METADATA)
#define MM_HEADER_SIZE MM_ALIGN_UP(sizeof(header_t))
#define MM_METADATA_SIZE MM_HEADER_SIZE

#define MM_MIN_PAYLOAD (MM_ALIGN_UP(MM_PAYLOAD_PTRS * sizeof(void*)))
#define MM_MIN_SPLIT (2 * MM_ALIGNMENT)
#define MM_MIN_BLOCK_SPLIT (MM_MIN_SPLIT + MM_METADATA_SIZE)

#define MM_FREE_BIT 0x1
#define MM_MMAP_BIT 0x2

#define MM_REG_MASK ~((size_t)MM_REG_SIZE - 1)
#define MM_SLAB_MASK ~((size_t)MM_SLAB_SIZE - 1)
#define MM_SLAB_IDX_MASK ((MM_REG_SIZE - 1) ^ (MM_SLAB_SIZE - 1))
#define MM_SLAB_SHIFT (__builtin_ctzll(MM_SLAB_IDX_MASK))
#define MM_FLAG_MASK ((size_t)MM_ALIGNMENT - 1)
#define MM_SIZE_MASK (~MM_FLAG_MASK)
#define MM_FREE_MASK (~MM_FREE_BIT)
#define MM_MMAP_MASK (~MM_MMAP_BIT)

static inline free_map_t mm_bin_bit(size_t i) { return (free_map_t)1 << (i); }
static inline void* mm_reg_align(void* ptr) {
	return (void*)(((uintptr_t)ptr + MM_REG_SIZE - 1) & ~((uintptr_t)MM_REG_SIZE - 1));
}

/*
 * mm_get_size(b): extracts payload size from header
 * mm_clr_flags(s): clears all flags/returns raw size
 */

static inline size_t mm_get_size(header_t* h) { return h->size & MM_SIZE_MASK; }
static inline size_t mm_clr_flags(size_t s) { return s & MM_SIZE_MASK; }
static inline _Bool mm_is_free(header_t* h) { return (h->size & MM_FREE_BIT) != 0; }
static inline _Bool mm_is_mmap(header_t* h) { return (h->size & MM_MMAP_BIT) != 0; }

/*
 * set_ free/mmap set bit
 * set_x free/mmap clear all bits then set
 * clr_ free/mmap remove bit
 */

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

static inline header_t** mm_get_next_ptr(header_t* h) { return (header_t**)((uint8_t*)h + MM_HEADER_SIZE); }
static inline void mm_set_next(header_t* h, header_t* next) { *((header_t**)((uint8_t*)h + MM_HEADER_SIZE)) = next; }
static inline header_t** mm_get_prev_ptr(header_t* h) {
	return (header_t**)((uint8_t*)h + MM_HEADER_SIZE + sizeof(void*));
}
static inline void mm_set_prev(header_t* h, header_t* prev) {
	*((header_t**)((uint8_t*)h + MM_HEADER_SIZE + sizeof(void*))) = prev;
}

static inline header_t* mm_get_next(header_t* h) { return *mm_get_next_ptr(h); }
static inline header_t* mm_get_prev(header_t* h) { return *mm_get_prev_ptr(h); }

static inline region_t* mm_get_reg(void* ptr) { return (region_t*)((uintptr_t)ptr & MM_REG_MASK); }
static inline void* mm_get_reg_end(region_t* reg) { return (void*)((uint8_t*)reg + MM_REG_SIZE); }
static inline void* mm_get_reg_start(region_t* reg) { return (void*)((uint8_t*)reg + MM_REG_METADATA); }

static inline void* mm_get_slab(void* ptr) { return (slab_t*)((uintptr_t)ptr & MM_SLAB_MASK); }
static inline int mm_get_slab_idx(void* ptr) { return (int)(((uintptr_t)ptr & MM_SLAB_IDX_MASK) >> MM_SLAB_SHIFT) - 1; }
static inline slab_t* mm_get_slab_metadata(void* ptr) {
	return &((slab_region_t*)mm_get_reg(ptr))->slabs_metadata[mm_get_slab_idx(ptr)];
}
static inline void* mm_get_slab_block(slab_t* slab, size_t idx) {
	return (void*)((uint8_t*)slab->start + (idx * slab->block_size));
}
static inline void* mm_get_slab_mem_from_idx(slab_region_t* reg, size_t idx) {
	return (void*)((uint8_t*)reg + (idx + 1) * MM_SLAB_SIZE);
}

static inline slab_list_t* mm_get_slab_list(slab_t* slab) {
	return &mm_get_reg(slab)->arena->slab_lists[slab->block_size / MM_SLAB_BASE - 1];
}

static inline size_t mm_get_bitmap_count(size_t size) { return ((size_t)MM_SLAB_SIZE / size) / 64; }

static inline header_t* mm_header(void* payload) { return (header_t*)((uint8_t*)payload - MM_HEADER_SIZE); }
static inline size_t* mm_canary(header_t* h) { return (size_t*)((uint8_t*)h + MM_HEADER_SIZE + mm_get_size(h)); }
static inline void* mm_payload(header_t* h) { return (void*)((uint8_t*)h + MM_HEADER_SIZE); }
static inline header_t* mm_next_header(header_t* h) {
	return (header_t*)((uint8_t*)h + mm_get_size(h) + MM_METADATA_SIZE);
}
static inline void mm_link_next_header(header_t* h) { mm_next_header(h)->prev = h; }

static inline size_t mm_floor_log2(size_t x) { return 63 - __builtin_clzll(x); }
static inline size_t mm_ceil_log2(size_t x) { return 64 - __builtin_clzll(x - 1); }

#define MM_ABORT() __builtin_trap()

// arena.c
_Bool mm_init_arena(void);
_Bool mm_grow_arena(arena_t* arena);
_Bool mm_grow_slabs(arena_t* arena);
void* mm_mmap_alloc(size_t size);
void mm_mmap_free(header_t* header);

// free_list.c
size_t mm_idx_from_size(size_t s);
size_t mm_size_from_idx(size_t i);
void mm_add_to_free(header_t* h);
_Bool mm_remove_free(header_t* h);
header_t* mm_find_fit(size_t size, arena_t* arena);

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

// slabs.c
void* mm_free_slab(slab_t* slab);
void* mm_slab_malloc(size_t size, arena_t* arena);
void* mm_slab_free(void* ptr);

// asserts
_Static_assert((MM_REG_SIZE & (MM_REG_SIZE - 1)) == 0, "MM_REG_SIZE must be a power of two");
_Static_assert((MM_SLAB_SIZE & (MM_SLAB_SIZE - 1)) == 0, "MM_SLAB_SIZE must be a power of two");
_Static_assert((MM_ALIGNMENT & (MM_ALIGNMENT - 1)) == 0, "MM_ALIGNMENT must be a power of two");
_Static_assert((MM_SLAB_SIZE / MM_SLAB_THRESHOLD) % 64 == 0,
			   "Slabs per region must be a multiple of 64 for bitmap alignment");

#endif
