# Custom C Allocator
A custom implementation of malloc/free/realloc/calloc using segregated free lists and an sbrk heap.

## Features
- **Segregated free lists** - 32 size classes with bitmap search
- **Hybrid size classes** - Exponential bins starting from 16 bytes
- **Boundary-tag coalescing** - Bidirectional coalescing using pointers in the header (no footers)
- **mmap for large allocations** - Allocations ≥128KiB bypass the heap and get their own page-aligned block of memory
- **Debug mode** - Canaries, poisoning, and integrity checks

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

## Memory Management
- **Small allocations** (<128KiB): Allocated from sbrk heap
- **Large allocations** (≥128KiB): Direct mmap allocation
- **Heap growth**: Doubles each time, starting at 4KiB
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
- mmap blocks never participate in coalescing
- Flag bits (FREE_BIT, MMAP_BIT) encoded in low bits of size

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
- Linux only (uses sbrk, specific mmap flags)
- Single-threaded (no locks or thread-local arenas)
- Requires GCC/Clang (uses `__builtin_ctz`, `__builtin_clzll`)

## TODO
- [ ] Region-based arenas (replace global sbrk heap)
- [ ] Thread-safety (per-thread arenas)
- [ ] Improve test coverage
- [ ] Add benchmarking suite

## Non-Goals
- Production use or performance competitive with glibc/jemalloc
- Lock-free data structures
- NUMA awareness
