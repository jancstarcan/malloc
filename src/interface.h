/*
 * Internal allocator implementation details.
 * Do not include outside allocator source files.
 */

#ifndef MM_PRIVATE_HEADER
#define MM_PRIVATE_HEADER

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

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
 *   - In debug mode the next pointer is part of header_t
 *   - In release mode it's stored in the payload
 *   - Either way GET/SET _NEXT should be used
 *
 * Block layout:
 *
 *   Normal:
 *     [ Header | Payload | Footer ]
 *
 *   DEBUG:
 *     [ Header | Payload | Canary | Footer ]
 *
 * Invariants:
 *   - Free blocks must be in the free list
 *   - Header and footer must always match
 *   - The free list must not include duplicates
 *   - Footer stores payload size WITHOUT flags.
 *   - All blocks are MM_ALIGNMENT-aligned
 *   - mmap allocated blocks don't have footers
 */

#ifdef MM_DEBUG
typedef struct header {
	size_t size;
	struct header* next;
} header_t;
#else
typedef struct {
	size_t size;
} header_t;
#endif

typedef struct {
	size_t size;
} footer_t;

#define MM_BIN_COUNT 32
#define MM_BIN_BASE MM_ALIGNMENT
extern header_t* free_lists[MM_BIN_COUNT];

#if MM_BIN_COUNT <= 8
typedef uint8_t bins_map_t;
#elif MM_BIN_COUNT <= 16
typedef uint16_t bins_map_t;
#elif MM_BIN_COUNT <= 32
typedef uint32_t bins_map_t;
#elif MM_BIN_COUNT < 64
typedef uint64_t bins_map_t;
#else
#error "Too many bins for bitmap"
#endif

extern bins_map_t bins_map;

/*
 * Heap state:
 *   heap_start pointer to the start of sbrk heap
 *   heap_end pointer to the byte after the end of sbrk heap
 *   heap_initialized must be true before allocations
 *   heap_size reflects current heap_end - heap_start
 */

extern void* heap_start;
extern void* heap_end;
extern size_t heap_size;
extern _Bool heap_initialized;

#define MMAP_THRESHOLD (128 * 1024)
#define INITIAL_HEAP_SIZE 4096

#define MM_ALIGNMENT alignof(max_align_t)
#define MM_ALIGN_UP(x) (((x) + MM_ALIGNMENT - 1) & ~(MM_ALIGNMENT - 1))

#define MM_HEADER_SIZE MM_ALIGN_UP(sizeof(header_t))
#define MM_FOOTER_SIZE MM_ALIGN_UP(sizeof(footer_t))

#ifdef MM_DEBUG
#define MM_CANARY_BYTE 0xCC
#define MM_POISON_FREE_BYTE 0xDD
#define MM_POISON_ALLOC_BYTE 0xAA
#define MM_CANARY_SIZE MM_ALIGN_UP(sizeof(size_t))
#define ENABLE_CANARIES
#define ENABLE_POISONING
#else
#define MM_CANARY_SIZE 0
#endif

#define MM_MIN_PAYLOAD (2 * MM_ALIGNMENT)
#define MM_MIN_BLOCK_SIZE (MM_HEADER_SIZE + MM_MIN_PAYLOAD + MM_CANARY_SIZE + MM_FOOTER_SIZE)

#define MM_FREE_BIT 0x1
#define MM_MMAP_BIT 0x2

#define MM_FLAG_MASK ((size_t)(MM_ALIGNMENT - 1))
#define MM_SIZE_MASK (~MM_FLAG_MASK)
#define MM_FREE_MASK (~MM_FREE_BIT)
#define MM_MMAP_MASK (~MM_MMAP_BIT)

#define MM_BIN_BIT(i) ((bins_map_t)1 << (i))

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
 *   - Intact headers and footers
 *   - Undefined behavior if heap is corrupted
 *   - PREV_ MM_HEADER/MM_FOOTER assumes it's not the first block
 *   - NEXT_ MM_HEADER/MM_FOOTER assumes it's not the last block
 *
 * These macros will gladly read junk if the wrong pointer is provided
 */

#ifdef MM_DEBUG
#define MM_GET_NEXT(h) ((h)->next)
#define MM_SET_NEXT(h, n) ((h)->next = n)
#else
#define MM_GET_NEXT(h) (*(header_t**)((uint8_t*)(h) + MM_HEADER_SIZE))
#define MM_SET_NEXT(h, n) (*((header_t**)((uint8_t*)(h) + MM_HEADER_SIZE)) = (n))
#endif

#define MM_HEADER(p) ((header_t*)((uint8_t*)(p) - MM_HEADER_SIZE))
#define MM_CANARY(h) ((size_t*)((uint8_t*)(h) + MM_HEADER_SIZE + MM_GET_SIZE(h)))
#define MM_FOOTER(h) ((footer_t*)((uint8_t*)(h) + MM_HEADER_SIZE + MM_GET_SIZE(h) + MM_CANARY_SIZE))
#define MM_PAYLOAD(h) ((void*)((uint8_t*)(h) + MM_HEADER_SIZE))
#define MM_PREV_FOOTER(h) ((footer_t*)((uint8_t*)(h) - MM_FOOTER_SIZE))
#define MM_PREV_HEADER(h) ((header_t*)((uint8_t*)MM_PREV_FOOTER(h) - MM_CANARY_SIZE - MM_PREV_FOOTER(h)->size - MM_HEADER_SIZE))
#define MM_NEXT_HEADER(h) ((header_t*)((uint8_t*)(h) + MM_HEADER_SIZE + MM_GET_SIZE(h) + MM_CANARY_SIZE + MM_FOOTER_SIZE))

#define abort() __builtin_trap()

/*
 * Function declarations
 *
 * Notes:
 *   - grow_heap doubles sbrk heap size
 *   - coalesce_* remove merged neighbors from free_list
 *   - caller must reinsert the resulting block
 */

// debug.c
void debug_test(void);
void write_canary(header_t* h);
void check_canary(header_t* h);
void poison_free(void* p);
void poison_alloc(void* p);
void poison_free_area(void* p, size_t s);
void poison_alloc_area(void* p, size_t s);
void heap_check(void);
void free_check(void);

#ifdef MM_DEBUG
#define MM_RUN_CHECKS() debug_test()
#else
#define MM_RUN_CHECKS() ((void)0)
#endif

// heap.c
_Bool init_heap(void);
_Bool grow_heap(void);
void* mmap_alloc(size_t size);
void mmap_free(header_t* header);

// free_list.c
void add_to_free(header_t* h);
_Bool remove_free(header_t* h);
header_t* find_fit(size_t size);

// block.c
void coalesce_prev(header_t** header_ptr);
void coalesce_next(header_t* header);
void shrink_block(header_t* header, size_t size);
_Bool try_grow_in_place(header_t* header, size_t size);
void* malloc_block(size_t size);

// mem.c
void* malloc(size_t size);
void* realloc(void* ptr, size_t size);
void* calloc(size_t size, size_t n);
void free(void* ptr);

// stats.c
void add_alloced(size_t n, _Bool mmap);
void print_alloced(void);
void print_free(void);
void print_stats(void);

#endif
