# Custom C allocator

## Features
- sbrk heap
- mmap for large allocations
- coalescing
- debug mode checks (heap/free consistency)

## Block layout

Normal:
\[header]\[payload]\[footer]

## Invariants
- All blocks in the free list are free
- Allocated blocks must not be in the free list
- Headers and footers must match for heap blocks
- The free list must not include duplicates
- Blocks must not overlap

## Free list
Singly linked, first-fit

## Build
- __make debug__ for debug mode, compiler checks and debugging labels
- __make__ for the optimized build

## Tests
./test.bin

## Debugging
- heap_check
- free_check

## TODO
- canaries
- poisoning
- bins
