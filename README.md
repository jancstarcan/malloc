# Custom C Allocator
A custom implementation of malloc/free/realloc/calloc using segregated free lists and mmap arenas.

## Features
- **Segregated free lists** - 32 size classes with bitmap search
- **Size classes** - Exponential bins starting from 16 bytes
- **Boundary-tag coalescing** - Each block header stores a pointer to the previous physical block, allowing backward traversal without footers.
- **Slabs for small allocations** - Slabs are used for smaller allocations. This eliminates the need for per-block metadata, coalescing and a free list
- **mmap for large allocations** - Large allocations bypass the heap and get their own page-aligned block of memory

## Block Layout
| Header | Payload |
|--------|---------|

Next and previous pointers are stored in the payload

Headers contain a `size_t` size with encoded flags and a pointer to the previous block

## Slab Layout
| region metadata \| slab bitmap \| slab\_metadata |
|--------------------------------------------------|
| Slab 1                                           |
| Slab 2                                           |
| ...                                              |

Since the first would-be slab is wasted due to alignment constraints,
it's repurposed to store the metadata for all slabs.
This saves `sizeof(slab_t)` bytes per slab.

All slabs are `MM_SLAB_SIZE` aligned, this allows for O(1) slab lookup.

## Memory Management
- **Arena growth**: Starts at `MM_REG_SIZE`, mmap calls double in size each time, up to `MM_REG_CAP`
- **Small allocations**: Allocated from slabs
- **Medium allocations**: Allocated from general-purpose regions
- **Large allocations**: Direct mmap allocation, munmap'd on free
- **Coalescing**: Forward and backward coalescing on every free

The sizes are defined by `MM_SLAB_THRESHOLD` and `MM_MMAP_THRESHOLD`, and may change.

## Free List
The allocator uses segregated free lists with variable-size bins:

- **Bin layout**:
  - Starts at `MM_SLAB_THRESHOLD` bytes (smallest bin not included)
  - Each bin class doubles in base size
  - Each bin class is subdivided into 4 evenly spaced steps
  - Blocks are fixed to the free list's size classes
- **Allocation**:
  - Each allocation is rounded up to the nearest size class
  - No scanning of the free list is required
  - O(1) lookup using a bitmap and bit operations (`__builtin_ctz()`)

**Example (first bin class starting at 512 bytes with 4 subdivisions):**

| Bin | Block Size (bytes) |
|-----|------------------|
| 1   | 640              |
| 2   | 768              |
| 3   | 896              |
| 4   | 1024             |
| ... | ...              |

## Debug Mode
Debug mode was removed in favor of performance and simpler layout

When compiled with `-DMM_DEBUG` it used to have:
- **Canaries**: Detect buffer overflows
- **Poisoning**: 0xAA for allocated, 0xDD for freed memory
- **Heap integrity checks**: Verify all block headers/prev pointers
- **Free list checks**: Ensures all free blocks are properly marked
- **Statistics**: Tracks heap size, allocation count, bytes allocated

Debug mode couldn't work well with slabs - it would either be incomplete or significantly slower.
For debugging, use Valgrind or AddressSanitizer. The old debug implementation is available in tag v1.3 and earlier.

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
> Debug builds use -O0 -g with stricter warnings

## Tests
```bash
./test.bin                    # Run test suite
LD_PRELOAD=./malloc.so <cmd>  # Test with real programs (single-threaded only)
```

## Known Limitations
- Linux only (uses mmap)
- Requires GCC/Clang (uses `__builtin_ctz`, `__builtin_clz`)
- Single-threaded (no locks or thread-local arenas)

## TODO
- [X] Region-based arenas (replace global sbrk heap)
- [X] Slabs for small allocations
- [X] Improve free lists
- [ ] Improve test coverage
- [ ] Add some benchmarking
- [ ] Thread-safety (per-thread arenas)

## Non-Goals
- Production use or performance competitive with glibc/jemalloc
- Lock-free data structures
- NUMA awareness
