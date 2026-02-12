# Custom C allocator
This allocator uses a segregated free list design over an sbrk heap, or mmap for larger allocations. Blocks contain headers and footers to support constant-time coalescing. In debug mode, additional integrity checks, canaries, and payload poisoning are enabled.

## Features
- sbrk heap
- mmap for large allocations
- coalescing
- segregated free lists
- debug mode

## Debug mode
- Consistency checks between header and footer for the entire heap
- Checks that every block in the free list is marked free
- Enables canaries and payload poisoning
- Keeps track of how much memory was allocated

### Statistics
- In debug mode it keeps track of:
  - Heap size
  - Number of allocations
  - Number of allocated bytes

## Block layout
- Normal:
   | Header | Payload | Footer |
   |--------|---------|--------|

   Next pointer is stored in the payload

- Debug:
   | Header | Payload | Canary | Footer |
   |--------|---------|--------|--------|

   Next pointer is stored in the header

## Flag encoding
- There are two flags encoded in the low bits of the header's size:
  - bit 0: mark block as free
  - bit 1: mark block as mmap-allocated
- Footers store the size without flags

## Memory management
- An sbrk heap is used for allocations smaller than 128KiB.
- The heap grows geometrycally. Each extension doubles the previous size, starting from `INITIAL_HEAP_SIZE`
- mmap is used to allocate memory directly for allocations larger than 128KiB, so that it may be returned to the OS
- Coalescing occurs on every `free()`. Both the previous and next blocks are checked

## Free list
- Multiple segregated lists, each first-fit
- The size class ranges double with each list
- The number of lists can be specified in interface.h, with up to 63 lists

## Design invariants
- All blocks are `max_align_t` aligned
- Free blocks appear in exactly one free list
- Header and footer sizes must match (except for mmap blocks)
- mmap never participate in coalescing

## Edge cases
- `malloc(0)` -> `NULL`
- `free(NULL)` -> no-op
- `realloc(NULL, size)` -> `malloc(size)`
- `realloc(ptr, 0)` -> `free(ptr)`

## Build
- `make debug` - Debug test build
- `make release` - Optimized test build
- `make ddynlib` - Debug dynamically linked library
- `make rdynlib` - Optimized dynamically linked library
- `make dstatlib` - Debug statically linked library
- `make rstatlib` - Optimized statically linked library

## Tests
- ./test.bin

## TODO
- Improve test.c
- Add more debug mode checks and statistics

## Non-goals
- High-performance production use
- Lock-free or wait-free guarantees
- Thread-safety

## Known limitations
- It only works on Linux
- Most likely depends on GCC/Clang
