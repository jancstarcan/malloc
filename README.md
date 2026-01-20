# Custom C allocator

## Features
- sbrk heap
- mmap for large allocations
- coalescing
- debug mode

## Block layout
- Normal:
  +--------------------------------+
  | Header | Payload | Footer |
  +--------------------------------+

- Debug:
  +-------------------------------------------+
  | Header | Payload | Canary | Footer |
  +-------------------------------------------+

## Flag encoding
- There are two flags encoded in the lowest bits of the header's size:
  - One to mark it as free
  - The other to signify it was allocated via mmap

## Memory management
- An sbrk heap is used for allocations smaller than 128KiB. The heap doubles in size when extended
- mmap is used to allocate memory directly for allocations larger than 128KiB, so that it may be returned to the OS

## Free list
- Singly linked, first-fit

## Build
- __make debug__ for debug mode, compiler checks and debugging labels
- __make release__ for the optimized build

## Tests
- ./test.bin

## Debug mode
- Consistency checks between header and footer for the entire heap
- Checks that every block in the free list is marked free
- Detects some use-after-free errors
- Enables canaries

## Known limitations
- The allocator isn't very efficient once the heap becomes fragmented, cause it uses a first-fit search through the free list
  Two possible fixes would be switching to best-fit (slow), or implementing bins (more complex)
- It doesn't have

## TODO
1. Poisoning:
   - Poisoning the payload can be a good way to detect an uninitialized value or use-after-free
2. Move header size to the payload:
   - This would reduce the header's size down to just a size\_t. Might be insignificant depending on max\_align\_t
3. Bins:
   - This would greatly improve the allocators performance when the heap is fragmented, while not wasting as much memory as a first-fit approach
