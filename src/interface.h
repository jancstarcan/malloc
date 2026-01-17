#ifndef INTERFACE
#define INTERFACE

#include <stddef.h>
#include <stdint.h>

typedef struct Header {
	size_t size;
	struct Header* next;
} Header;

extern Header* free_list;

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

#define MIN_PAYLOAD (2 * ALIGNMENT)
#define MIN_BLOCK_SIZE (HEADER_SIZE + MIN_PAYLOAD + FOOTER_SIZE)

#define FREE_BIT 0x1
#define MMAP_BIT 0x2

#define SIZE_MASK (~(ALIGNMENT - 1))
#define FREE_MASK (~FREE_BIT)
#define MMAP_MASK (~MMAP_BIT)

#define GET_SIZE(b) ((b)->size & SIZE_MASK)
#define SGET_SIZE(s) ((s) & SIZE_MASK)
#define IS_FREE(b) (((b)->size & FREE_BIT) != 0)
#define IS_MMAP(b) (((b)->size & MMAP_BIT) != 0)

#define SET_FREE(s) ((s) | FREE_BIT)
#define SET_MMAP(s) ((s) | MMAP_BIT)
#define SET_USED(s) ((s) & FREE_MASK)
#define CLR_MMAP(s) ((s) & MMAP_MASK)

#define SET_XFREE(s) (SET_FREE(SGET_SIZE((s))))
#define SET_XMMAP(s) (SET_MMAP(SGET_SIZE((s))))
#define SET_XUSED(s) (SET_USED(SGET_SIZE((s))))
#define CLR_XMMAP(s) (CLR_MMAP(SGET_SIZE((s))))

#define HEADER(p) ((Header*)((uint8_t*)(p) - HEADER_SIZE))
#define FOOTER(b) ((size_t*)((uint8_t*)(b) + HEADER_SIZE + GET_SIZE(b)))
#define PREV_FOOTER(b) ((size_t*)((uint8_t*)(b) - FOOTER_SIZE))
#define PREV_HEADER(b)                                                         \
	((Header*)((uint8_t*)PREV_FOOTER(b) - *PREV_FOOTER(b) - HEADER_SIZE))
#define NEXT_HEADER(b)                                                         \
	((Header*)((uint8_t*)(b) + HEADER_SIZE + GET_SIZE(b) + FOOTER_SIZE))

// Debug
void heap_check(void);

// Heap
_Bool init_heap(void);
_Bool grow_heap(void);
void* mmap_alloc(size_t size);
void mmap_free(Header* header);

// Free list
void add_to_free(Header* header);
_Bool remove_free(Header* header);
Header* find_fit(size_t size);

// Block
void coalesce_prev(Header** header_ptr);
void coalesce_next(Header* header);
void shrink_block(Header* header, size_t size);
_Bool try_grow_in_place(Header* header, size_t size);
void* malloc_block(size_t size);

// Malloc
void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t size, size_t n);

#endif
