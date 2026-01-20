# Custom C allocator

## Features
- sbrk heap
- mmap for large allocations
- coalescing
- debug mode:
  - heap and free list consistency checks
  - canaries

## Block layout

Normal:
[header][payload][footer]

Debug:
[header][payload][canary][footer]

## Free list
Singly linked, first-fit

## Build
- __make debug__ for debug mode, compiler checks and debugging labels
- __make release__ for the optimized build

## Tests
./test.bin

## Debugging
- heap_check
- free_check
- check_canary

## TODO
- poisoning
- bins
