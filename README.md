# Custom C allocator

## Features
- sbrk heap
- mmap for large allocations
- coalescing
- debug mode checks

## Block layout
\[header\]\[payload\]\[footer\]

# Invariants

- Free blocks __must__ be in the free list
- Allocated blocks __must not__ be in the free list
- Headers and footers must always match
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
- canaries
- poisoning

## TODO
- bins

