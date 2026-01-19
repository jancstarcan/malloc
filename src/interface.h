/*
 * Internal allocator implementation details.
 * Do not include outside allocator source files.
 */

#ifndef MALLOC_PRIVATE_HEADER
#define MALLOC_PRIVATE_HEADER

#include <stddef.h>
#include <stdint.h>

/*
 * INTERNAL ALLOCATOR LAYOUT
 *
 * Header->size encodes:
 *   - payload size (aligned)
 *   - FREE_BIT (is free)
 *   - MMAP_BIT (is allocated with mmap)
 *
 * Block layout:
 *
 *   Normal:
 *     [ Header | Payload | Footer ]
 *
 *   DEBUG:
 *     [ Header | Payload | Canaries | Footer ]
 *
 * Invariants:
 *   - Free blocks must be in the free list
 *   - Header and footers must always match
 *   - The free list must not include duplicates
 *   - Footer stores payload size WITHOUT flags.
 *   - All blocks are ALIGNMENT-aligned
 */

typedef struct Header {
	size_t size;
	struct Header* next;
} Header;

// Singly linked list
extern Header* free_list;

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

#define MMAP_THRESHOLD 131072
#define INITIAL_HEAP_SIZE 4096

#define ALIGNMENT sizeof(max_align_t)
#define ALIGN_UP(x) (((x) + ALIGNMENT - 1) & ~(ALIGNMENT - 1))

#define HEADER_SIZE ALIGN_UP(sizeof(Header))
#define FOOTER_SIZE ALIGN_UP(sizeof(size_t))

#ifdef DEBUG
#define CANARY_SIZE ALIGN_UP(sizeof(size_t))
#define ENABLE_CANARIES 1
#else
#define CANARY_SIZE 0
#endif

#define MIN_PAYLOAD (2 * ALIGNMENT)
#define MIN_BLOCK_SIZE (HEADER_SIZE + MIN_PAYLOAD + FOOTER_SIZE)

#define FREE_BIT 0x1
#define MMAP_BIT 0x2

#define SIZE_MASK (~(ALIGNMENT - 1))
#define FREE_MASK (~FREE_BIT)
#define MMAP_MASK (~MMAP_BIT)

/*
 * Pointer macros assume:
 *   - Valid, in-heap block headers
 *   - Intact headers and footers
 *   - Undefined behavior if heap is corrupted
 *   - PREV_HEADER assumes a valid footer before the block
 */

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

#define HEADER(p) ((Header*)((uint8_t*)(p) - HEADER_SIZE))
#define FOOTER(b) ((size_t*)((uint8_t*)(b) + HEADER_SIZE + GET_SIZE(b)))
#define PREV_FOOTER(b) ((size_t*)((uint8_t*)(b) - FOOTER_SIZE))
#define PREV_HEADER(b)                                                         \
	((Header*)((uint8_t*)PREV_FOOTER(b) - *PREV_FOOTER(b) - HEADER_SIZE))
#define NEXT_HEADER(b)                                                         \
	((Header*)((uint8_t*)(b) + HEADER_SIZE + GET_SIZE(b) + FOOTER_SIZE))

// Function declarations

/*
 * Notes:
 *   - grow_heap doubles sbrk heap size
 *   - coalesce_* remove merged neighbors from free_list
 *   - caller must reinsert the resulting block
 */

// debug.c
void debug_test(void);
void heap_check(void);
void free_check(void);
void canary_check(void);
void poisoning_check(void);

#ifdef DEBUG
#define RUN_CHECKS() debug_test()
#else
#define RUN_CHECKS() ((void)0)
#endif

// heap.c
_Bool init_heap(void);
_Bool grow_heap(void);
void* mmap_alloc(size_t size);
void mmap_free(Header* header);

// free_list.c
void add_to_free(Header* header);
_Bool remove_free(Header* header);
Header* find_fit(size_t size);

// block.c
void coalesce_prev(Header** header_ptr);
void coalesce_next(Header* header);
void shrink_block(Header* header, size_t size);
_Bool try_grow_in_place(Header* header, size_t size);
void* malloc_block(size_t size);

// mem.c
void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t size, size_t n);

#endif
