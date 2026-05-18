# Malloc Lab: Explicit Free List Allocator

In this lab, you will convert an implicit free list allocator to an explicit free list allocator with advanced optimizations.

## Getting Started

1. Read through `src/mm-explicit.c` to understand the provided starter code
2. Build the project: `make`
3. Run the test driver: `./bin/mdriver-explicit`

The starter code provides:
- Basic implicit free list implementation (similar to what you completed in the previous lab)
- Block structure with headers
- Helper functions for block manipulation
- Memory system simulator

## Your Tasks

### Task 1: Convert to Explicit Free List (Correctness)

**Current state:** Your code from part a uses an implicit free list - it traverses ALL blocks in the heap to find free space.

**Your goal:** Convert to an explicit free list - only traverse FREE blocks.

**Implementation:**
1. Update the `block_t` structure to include `next` and `prev` pointers (inside a union with payload)
2. Maintain a global `free_list_head` pointer
3. Implement `list_add()` - add freed blocks to the front of the free list (LIFO)
4. Implement `list_remove()` - remove blocks from the free list when allocating
5. Update `find_fit()` - traverse only the explicit free list, not all blocks
6. Update `mm_free()` - add freed blocks to the free list

**Expected outcome:** All tests pass, but score may be low (~20-30/60)

---

### Task 2: Add Boundary Tags (Coalescing)

**Current state:** Free blocks aren't merged, causing external fragmentation.

**Your goal:** Add footers (boundary tags) to enable bidirectional coalescing.

**Implementation:**
1. Add footer helper functions:
   - `get_footer()` - get pointer to block's footer
   - `set_footer()` - write footer to match header
   - `get_prev_block()` - use footer to find previous block in memory
2. Update size calculations to account for footer space (add `sizeof(size_t)` to all size calculations)
3. Update `mm_free()` to coalesce with adjacent free blocks:
   - Check if next block is free and merge (forward coalescing)
   - Check if previous block is free and merge (backward coalescing)
   - Add merged block to free list

**Expected outcome:** Score ~50-55/60, much better space utilization

---

### Task 3: Optimize Find-Fit for Gradescope (Performance)

**Current state:** Implicit list traversal is too slow.

**Your goal:** Use first-fit on the explicit free list for maximum speed.

**Implementation:**
1. Update `find_fit()` to traverse only the explicit free list (not all blocks!)
2. Return the first block that fits (first-fit policy)
3. This is much faster than traversing all blocks or using best-fit

**Expected outcome:** Score 60/60 on Gradescope (perfect!)

---

## Testing

Run the test driver to check your implementation:

```bash
make
./bin/mdriver-explicit
```

The driver runs 12 trace files that simulate various allocation patterns:
- `corners.rep` - Edge cases
- `short2.rep` - Simple allocations
- `malloc.rep` - Basic malloc patterns
- `coalescing-bal.rep` - Tests coalescing (Task 2)
- `fs.rep`, `hostname.rep`, `login.rep`, `ls.rep`, `perl.rep`, `random-bal.rep`, `rm.rep`, `xterm.rep` - Real program traces

**Score breakdown:**
- Correctness: Must pass all tests (Task 1)
- Utilization: Space efficiency (Tasks 2 & 3)
- Throughput: Speed (explicit list + best-fit)
- Final score: 0-60 points

## Key Concepts

**Explicit Free List:**
- Maintain a linked list of only free blocks (not all blocks)
- Each free block has next/prev pointers
- Much faster than implicit list (O(# free blocks) vs O(# all blocks))

**Boundary Tags (Footers):**
- Store block size at both start (header) and end (footer)
- Enables O(1) backward coalescing
- Footer = copy of header at end of block

**Immediate Coalescing:**
- Merge adjacent free blocks immediately in `mm_free()`
- Check both forward and backward neighbors
- Reduces external fragmentation

**Best-Fit Allocation:**
- Search free list for smallest block that fits
- Reduces fragmentation compared to first-fit
- Slightly slower but much better space utilization

## Tips

- Start with Task 1 - get the explicit list working first
- Test after each task to ensure you didn't break anything
- Draw diagrams of your heap structure on paper
- With footers, every block needs: header (8) + payload + footer (8)
- Minimum free block size: header (8) + next ptr (8) + prev ptr (8) + footer (8) = 32 bytes

## Submission

Submit your completed `mm-explicit.c` file.

**Expected scores:**
- Task 1 complete: Tests pass, explicit list working (~20-30/60)
- Task 2 complete: With coalescing (~50-55/60)
- Task 3 complete: With first-fit optimization (60/60 - perfect!)

Good luck!
