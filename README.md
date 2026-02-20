# Custom C Allocator
A custom implementation of malloc/free/realloc/calloc using segregated free lists and mmap arenas.

## Features
- **Segregated free lists** - 32 size classes with bitmap search
- **Size classes** - Exponential bins starting from 16 bytes
- **Boundary-tag coalescing** - Each block header stores a pointer to the previous physical block, allowing backward traversal without footers.
- **mmap for large allocations** - Allocations ≥128KiB bypass the heap and get their own page-aligned block of memory
- **Debug mode** - Canaries, poisoning, and integrity checks

## Structure
The individual allocations get their memory from regions. Regions are fixed size, 512KiB chunks of memory, stored in arenas.
Currently there's only one global arena, later they will be assigned to threads.

### Statistics
In debug mode it keeps track of:
- Heap size
- Number of allocations
- Number of allocated bytes

## Block Layout
**Normal:**

| Header | Payload |
|--------|---------|

Next and previous pointers are stored in the payload

**Debug:**

| Header | Payload | Canary |
|--------|---------|--------|

Next and previous pointers are stored in the header

Headers contain a size\_t size with encoded flags, a pointer to the previous block
and in debug mode only pointers to the previous and next free list blocks.

## Memory Management
- **Arena growth**: Starts with 512KiB, mmap calls double in size each time, up to 4MiB
- **Small allocations** (<128KiB): Allocated from arenas
- **Large allocations** (≥128KiB): Direct mmap allocation, munmap'd on free
- **Coalescing**: Forward and backward coalescing on every free

## Segregated Free Lists
- 32 bins tracked with a 32-bit bitmap
- Exponential size classes (powers of 2)
- First-fit allocation within each bin
- O(1) bin lookup using `__builtin_ctz()`

## Debug Features
When compiled with `-DMM_DEBUG`:
- **Canaries**: Detect buffer overflows
- **Poisoning**: 0xAA for allocated, 0xDD for freed memory
- **Heap integrity**: Verify all block headers/prev pointers
- **Free list checks**: Ensures all free blocks are properly marked
- **Statistics**: Tracks heap size, allocation count, bytes allocated

## Design Invariants
- All blocks are `max_align_t` aligned
- Free blocks appear in exactly one free list
- `prev` pointers must always be up-to-date
- Direct mmap allocations never participate in coalescing
- Flag bits (FREE\_BIT, MMAP\_BIT) encoded in low bits of size

## Edge Cases
- `malloc(0)` → `NULL`
- `free(NULL)` → no-op
- `realloc(NULL, size)` → `malloc(size)`
- `realloc(ptr, 0)` → `free(ptr); return NULL`

## Build
```bash
make debug      # Debug test build
make release    # Optimized test build
make ddynlib    # Debug shared library
make rdynlib    # Release shared library
make dstatlib   # Debug static library
make rstatlib   # Release static library
```

## Tests
```bash
./test.bin                    # Run test suite
LD_PRELOAD=./malloc.so <cmd>  # Test with real programs (single-threaded only)
```

## Known Limitations
- Linux only (uses mmap)
- Single-threaded (no locks or thread-local arenas)
- Requires GCC/Clang (uses `__builtin_ctz`, `__builtin_clzll`)

## TODO
- [X] Region-based arenas (replace global sbrk heap)
- [ ] Thread-safety (per-thread arenas)
- [ ] Improve test coverage
- [ ] Add benchmarking suite
- [ ] Implement some kind of defered coalescing

## Non-Goals
- Production use or performance competitive with glibc/jemalloc
- Lock-free data structures
- NUMA awareness
