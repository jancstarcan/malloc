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

#ifdef MM_DEBUG
typedef struct header {
	size_t size;
	struct header* prev;
	struct header* prev_free;
	struct header* next_free;
} header_t;
#define MM_PAYLOAD_PTRS 0
#else
typedef struct header {
	size_t size;
	struct header* prev;
} header_t;
#define MM_PAYLOAD_PTRS 2
#endif

#define MM_BIN_COUNT 32
#define MM_BIN_BASE MM_ALIGNMENT
extern header_t* mm_free_lists[MM_BIN_COUNT];

#if MM_BIN_COUNT <= 8
typedef uint8_t bins_map_t;
#elif MM_BIN_COUNT <= 16
typedef uint16_t bins_map_t;
#elif MM_BIN_COUNT <= 32
typedef uint32_t free_map_t;
#elif MM_BIN_COUNT < 64
typedef uint64_t bins_map_t;
#else
#error "Too many bins for bitmap"
#endif

extern free_map_t mm_free_map;

/*
 * Heap state:
 *   heap_start pointer to the start of sbrk heap
 *   heap_end pointer to the byte after the end of sbrk heap
 *   heap_initialized must be true before allocations
 *   heap_size reflects current heap_end - heap_start
 */

extern void* mm_heap_start;
extern void* mm_heap_end;
extern size_t mm_heap_size;
extern _Bool mm_heap_initialized;

#define MMAP_THRESHOLD (128 * 1024)
#define MM_INITIAL_HEAP_SIZE 4096

#define MM_ALIGNMENT alignof(max_align_t)
#define MM_ALIGN_UP(x) (((x) + MM_ALIGNMENT - 1) & ~(MM_ALIGNMENT - 1))

#define MM_PAGE_SIZE sysconf(_SC_PAGESIZE)
#define MM_PAGE_ALIGN(x) (((x) + MM_PAGE_SIZE - 1) & ~(MM_PAGE_SIZE - 1))

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

#define MM_HEADER_SIZE MM_ALIGN_UP(sizeof(header_t))
#define MM_METADATA_SIZE (MM_HEADER_SIZE + MM_CANARY_SIZE)

#define MM_MIN_PAYLOAD (MM_ALIGN_UP(MM_PAYLOAD_PTRS * sizeof(void*)))
#define MM_MIN_SPLIT (2 * MM_ALIGNMENT)
#define MM_MIN_BLOCK_SPLIT (MM_MIN_SPLIT + MM_METADATA_SIZE)

#define MM_FREE_BIT 0x1
#define MM_MMAP_BIT 0x2

#define MM_FLAG_MASK ((size_t)(MM_ALIGNMENT - 1))
#define MM_SIZE_MASK (~MM_FLAG_MASK)
#define MM_FREE_MASK (~MM_FREE_BIT)
#define MM_MMAP_MASK (~MM_MMAP_BIT)

#define MM_BIN_BIT(i) ((free_map_t)1 << (i))

/*
 * MM_GET_SIZE(b): extract payload size from header
 * MM_CLR_FLAGS(s): extract payload size from raw size_t
 */

#define MM_GET_SIZE(b) ((b)->size & MM_SIZE_MASK)
#define MM_CLR_FLAGS(s) ((s) & MM_SIZE_MASK)
#define MM_IS_FREE(b) (((b)->size & MM_FREE_BIT) != 0)
#define MM_IS_MMAP(b) (((b)->size & MM_MMAP_BIT) != 0)

/*
 * SET_ FREE/MMAP set bit
 * SET_X FREE/MMAP clear all bits then set
 * CLR_ FREE/MMAP remove bit
 */

#define MM_SET_FREE(s) ((s) | MM_FREE_BIT)
#define MM_SET_MMAP(s) ((s) | MM_MMAP_BIT)
#define MM_CLR_FREE(s) ((s) & MM_FREE_MASK)
#define MM_CLR_MMAP(s) ((s) & MM_MMAP_MASK)

#define MM_SET_XFREE(s) (MM_SET_FREE(MM_CLR_FLAGS((s))))
#define MM_SET_XMMAP(s) (MM_SET_MMAP(MM_CLR_FLAGS((s))))

/*
 * Pointer macros assume:
 *   - (h): a pointer to a header
 *   - (s): a pointer to a size_t
 *   - (b): a pointer to the payload
 *   - Undefined behavior if heap is corrupted
 *   - MM_PREV_ HEADER assumes it's not the first block
 *   - MM_NEXT_ HEADER assumes it's not the last block
 *
 * These macros will gladly read junk if they aren't used correctly,
 * all responsibility falls to the caller
 */

#ifdef MM_DEBUG
static inline header_t** MM_GET_NEXT_PTR(header_t* h) { return &h->next_free; }
static inline void MM_SET_NEXT(header_t* h, header_t* next) { h->next_free = next; }
static inline header_t** MM_GET_PREV_PTR(header_t* h) { return &h->prev_free; }
static inline void MM_SET_PREV(header_t* h, header_t* prev) { h->prev_free = prev; }
#else
static inline header_t** MM_GET_NEXT_PTR(header_t* h) { return (header_t**)((uint8_t*)h + MM_HEADER_SIZE); }
static inline void MM_SET_NEXT(header_t* h, header_t* next) { *((header_t**)((uint8_t*)h + MM_HEADER_SIZE)) = next; }
static inline header_t** MM_GET_PREV_PTR(header_t* h) { return (header_t**)((uint8_t*)h + MM_HEADER_SIZE + sizeof(void*)); }
static inline void MM_SET_PREV(header_t* h, header_t* prev) {
	*((header_t**)((uint8_t*)h + MM_HEADER_SIZE + sizeof(void*))) = prev;
}
#endif
static inline header_t* MM_GET_NEXT(header_t* h) { return *MM_GET_NEXT_PTR(h); }
static inline header_t* MM_GET_PREV(header_t* h) { return *MM_GET_PREV_PTR(h); }

// #define MM_HEADER(p) ((header_t*)((uint8_t*)(p) - MM_HEADER_SIZE))
// #define MM_CANARY(h) ((size_t*)((uint8_t*)(h) + MM_HEADER_SIZE + MM_GET_SIZE(h)))
// #define MM_PAYLOAD(h) ((void*)((uint8_t*)(h) + MM_HEADER_SIZE))
// #define MM_NEXT_HEADER(h) ((header_t*)((uint8_t*)(h) + MM_HEADER_SIZE + MM_GET_SIZE(h) + MM_CANARY_SIZE))
// #define MM_LINK_NEXT_HEADER(h) (MM_NEXT_HEADER(h)->prev = (h));
static inline header_t* MM_HEADER(void* payload) { return (header_t*)((uint8_t*)payload - MM_HEADER_SIZE); }
static inline size_t* MM_CANARY(header_t* h) { return (size_t*)((uint8_t*)h + MM_HEADER_SIZE + MM_GET_SIZE(h)); }
static inline void* MM_PAYLOAD(header_t* h) { return (void*)((uint8_t*)h + MM_HEADER_SIZE); }
static inline header_t* MM_NEXT_HEADER(header_t* h) {
	return (header_t*)((uint8_t*)h + MM_GET_SIZE(h) + MM_METADATA_SIZE);
}
static inline void MM_LINK_NEXT_HEADER(header_t* h) { MM_NEXT_HEADER(h)->prev = h; }
#define MM_MAX(a, b) (a > b ? a : b)

#define MM_ABORT() __builtin_trap()

/*
 * Function declarations
 *
 * Notes:
 *   - grow_heap doubles sbrk heap size
 *   - coalesce_* remove merged neighbors from free_list
 *   - caller must reinsert the resulting block
 */

// debug.c
void mm_debug_test(void);
void mm_write_canary(header_t* h);
void mm_check_canary(header_t* h);
void mm_poison_free(void* p);
void mm_poison_alloc(void* p);
void mm_poison_free_area(void* p, size_t s);
void mm_poison_alloc_area(void* p, size_t s);
void mm_heap_check(void);
void mm_free_check(void);

#ifdef MM_DEBUG
#define MM_RUN_CHECKS() mm_debug_test()
#else
#define MM_RUN_CHECKS() ((void)0)
#endif

// heap.c
_Bool mm_init_heap(void);
_Bool mm_grow_heap(void);
void* mm_mmap_alloc(size_t size);
void mm_mmap_free(header_t* header);
size_t mm_idx_from_size(size_t s);
size_t mm_size_from_idx(size_t i);

// free_list.c
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

#endif
