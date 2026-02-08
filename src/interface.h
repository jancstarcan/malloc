/*
 * Internal allocator implementation details.
 * Do not include outside allocator source files.
 */

#ifndef MALLOC_PRIVATE_HEADER
#define MALLOC_PRIVATE_HEADER

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

/*
 * INTERNAL ALLOCATOR LAYOUT
 *
 * Header->size encodes:
 *   - payload size (aligned)
 *   - MMAP_BIT (is allocated with mmap)
 *   - FREE_BIT (is free)
 *
 * Free list:
 *   - Multiple segregated lists
 *   - BIN_COUNT lists, up to 63
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
 *   - All blocks are ALIGNMENT-aligned
 *   - mmap allocated blocks don't have footers
 */

#ifdef DEBUG
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

#define BIN_COUNT 32
#define BIN_BASE ALIGNMENT
extern header_t* free_lists[BIN_COUNT];

#if BIN_COUNT <= 8
typedef uint8_t bins_map_t;
#elif BIN_COUNT <= 16
typedef uint16_t bins_map_t;
#elif BIN_COUNT <= 32
typedef uint32_t bins_map_t;
#elif BIN_COUNT < 64
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

#define ALIGNMENT alignof(max_align_t)
#define ALIGN_UP(x) (((x) + ALIGNMENT - 1) & ~(ALIGNMENT - 1))

#define HEADER_SIZE ALIGN_UP(sizeof(header_t))
#define FOOTER_SIZE ALIGN_UP(sizeof(footer_t))

#ifdef DEBUG
#define CANARY_BYTE 0xCC
#define POISON_FREE_BYTE 0xDD
#define POISON_ALLOC_BYTE 0xAA
#define CANARY_SIZE ALIGN_UP(sizeof(size_t))
#define ENABLE_CANARIES
#define ENABLE_POISONING
#else
#define CANARY_SIZE 0
#endif

#define MIN_PAYLOAD (2 * ALIGNMENT)
#define MIN_BLOCK_SIZE (HEADER_SIZE + MIN_PAYLOAD + CANARY_SIZE + FOOTER_SIZE)

#define FREE_BIT 0x1
#define MMAP_BIT 0x2

#define FLAG_MASK ((size_t)(ALIGNMENT - 1))
#define SIZE_MASK (~FLAG_MASK)
#define FREE_MASK (~FREE_BIT)
#define MMAP_MASK (~MMAP_BIT)

#define BIN_BIT(i) ((bins_map_t)1 << (i))

/*
 * GET_SIZE(b): extract payload size from header
 * CLR_FLAGS(s): extract payload size from raw size_t
 */

#define GET_SIZE(b) ((b)->size & SIZE_MASK)
#define CLR_FLAGS(s) ((s) & SIZE_MASK)
#define IS_FREE(b) (((b)->size & FREE_BIT) != 0)
#define IS_MMAP(b) (((b)->size & MMAP_BIT) != 0)

/*
 * SET_ FREE/MMAP set bit
 * SET_X FREE/MMAP clear all bits then set
 * CLR_ FREE/MMAP remove bit
 */

#define SET_FREE(s) ((s) | FREE_BIT)
#define SET_MMAP(s) ((s) | MMAP_BIT)
#define CLR_FREE(s) ((s) & FREE_MASK)
#define CLR_MMAP(s) ((s) & MMAP_MASK)

#define SET_XFREE(s) (SET_FREE(CLR_FLAGS((s))))
#define SET_XMMAP(s) (SET_MMAP(CLR_FLAGS((s))))

/*
 * Pointer macros assume:
 *   - (h): a pointer to a header
 *   - (s): a pointer to a size_t
 *   - Intact headers and footers
 *   - Undefined behavior if heap is corrupted
 *   - PREV_ HEADER/FOOTER assumes it's not the first block
 *   - NEXT_ HEADER/FOOTER assumes it's not the last block
 *
 * These macros will gladly read junk if the wrong pointer is provided
 */

#ifdef DEBUG
#define GET_NEXT(h) ((h)->next)
#define SET_NEXT(h, n) ((h)->next = n)
#else
#define GET_NEXT(h) (*(header_t**)((uint8_t*)(h) + HEADER_SIZE))
#define SET_NEXT(h, n) (*((header_t**)((uint8_t*)(h) + HEADER_SIZE)) = (n))
#endif

#define HEADER(p) ((header_t*)((uint8_t*)(p) - HEADER_SIZE))
#define CANARY(h) ((size_t*)((uint8_t*)(h) + HEADER_SIZE + GET_SIZE(h)))
#define FOOTER(h) ((footer_t*)((uint8_t*)(h) + HEADER_SIZE + GET_SIZE(h) + CANARY_SIZE))
#define PAYLOAD(h) ((void*)((uint8_t*)(h) + HEADER_SIZE))
#define PREV_FOOTER(h) ((footer_t*)((uint8_t*)(h) - FOOTER_SIZE))
#define PREV_HEADER(h) ((header_t*)((uint8_t*)PREV_FOOTER(h) - CANARY_SIZE - PREV_FOOTER(h)->size - HEADER_SIZE))
#define NEXT_HEADER(h) ((header_t*)((uint8_t*)(h) + HEADER_SIZE + GET_SIZE(h) + CANARY_SIZE + FOOTER_SIZE))

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

#ifdef DEBUG
#define RUN_CHECKS() debug_test()
#else
#define RUN_CHECKS() ((void)0)
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
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t size, size_t n);

// stats.c
void add_alloced(size_t n, _Bool mmap);
void dump_heap(void);
void dump_free_list(void);
void print_stats(void);

#endif
