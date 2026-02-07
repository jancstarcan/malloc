# Custom C allocator

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

## Block layout
- Normal:
   | Header | Payload | Footer |
   |--------|---------|--------|

- Debug:
   | Header | Payload | Canary | Footer |
   |--------|---------|--------|--------|

## Flag encoding
- There are two flags encoded in the lowest bits of the header's size:
  - One to mark it as free
  - The other to signify it was allocated via mmap

## Memory management
- An sbrk heap is used for allocations smaller than 128KiB. The heap doubles in size when extended
- mmap is used to allocate memory directly for allocations larger than 128KiB, so that it may be returned to the OS

## Free list
- Multiple segregated lists, first-fit
- The ranges double with each list
- The number of lists can be spcified in interface.h, up to 63 lists

## Build
- __make debug__ - Debug mode with checks, canaries, and stats
- __make release__ - Optimized production build
- __make dlib__ - Debug mode shared library
- __make rlib__ - Release mode shared library

## Tests
- ./test.bin

## Known limitations
- The allocator isn't very efficient once the heap becomes fragmented, cause it uses a first-fit search through the free list
  Two possible fixes would be switching to best-fit (slow), or implementing bins (more complex)

## TODO
