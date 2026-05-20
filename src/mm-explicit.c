/*
 * mm-implicit.c - The best malloc package EVAR!
 *
 * TODO (bug): mm_realloc and mm_calloc don't seem to be working...
 * TODO (bug): The allocator doesn't re-use space very well...
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"

/** The required alignment of heap payloads */
const size_t ALIGNMENT = 2 * sizeof(size_t);

typedef struct block_t block_t;
struct block_t {
    size_t header;
    union {
        struct {
            block_t* next;
            block_t* prev;
        };
        uint8_t payload[0];
    };
};

/** The first and last blocks on the heap */
static block_t* mm_heap_first = NULL;
static block_t* mm_heap_last = NULL;

static block_t* free_list_head = NULL;

void list_add(block_t* block) {
    block->prev = NULL;
    block->next = free_list_head;
    if (free_list_head) free_list_head->prev = block;
    free_list_head = block;
}

void list_remove(block_t* block) {
    if (!block) return;
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        free_list_head = block->next;  // block was the head
    }
    if (block->next) {
        block->next->prev = block->prev;
    }
}

/** Rounds up `size` to the nearest multiple of `n` */
static size_t round_up(size_t size, size_t n) {
    return (size + (n - 1)) / n * n;
}
static size_t get_size(block_t* block) { return block->header & ~1; }

static block_t* get_footer(block_t* block) {
    return (block_t*)((void*)block + (get_size(block) - sizeof(block_t)));
}

static void set_footer(block_t* block) {
    block_t* footer = get_footer(block);
    *footer = *block;
}

static block_t* get_prev_block(block_t* block) {
    block_t* prev_footer = (block_t*)((void*)block - sizeof(block_t));
    size_t size = get_size(prev_footer);
    return (block_t*)((void*)block - size);
}
/** Set's a block's header with the given size and allocation state */
static void set_header(block_t* block, size_t size, bool is_allocated) {
    block->header = size | is_allocated;
    set_footer(block);
}

/** Extracts a block's size from its header */

/** Extracts a block's allocation state from its header */
static bool is_allocated(block_t* block) { return block->header & 1; }

/**
 * Finds the first free block in the heap with at least the given size.
 * If no block is large enough, returns NULL.
 */
static block_t* find_fit(size_t size) {
    // Traverse the blocks in the heap using the implicit list
    for (block_t* curr = free_list_head; curr; curr = curr->next) {
        // If the block is free and large enough for the allocation, return it
        if (get_size(curr) >= size) {
            return curr;
        }
    }
    return NULL;
}

/** Gets the header corresponding to a given payload pointer */
static block_t* block_from_payload(void* ptr) {
    return ptr - offsetof(block_t, payload);
}

/**
 * mm_init - Initializes the allocator state
 */
bool mm_init(void) {
    // We want the first payload to start at ALIGNMENT bytes from the start of
    // the heap
    void* padding = mem_sbrk((3 * ALIGNMENT) - sizeof(block_t));
    if (padding == (void*)-1) {
        return false;
    }

    // Initialize the heap with no blocks
    mm_heap_first = NULL;
    mm_heap_last = NULL;
    free_list_head = NULL;
    return true;
}

/**
 * mm_malloc - Allocates a block with the given size
 */
void* mm_malloc(size_t size) {
    // printf("malloc\n");
    // The block must have enough space for a header and be 16-byte aligned
    size = round_up(2 * sizeof(block_t) + size, ALIGNMENT);

    // printf("Size of block_t: %zu \n", sizeof(block_t));
    //   coalesce();
    // If there is a large enough free block, use it
    block_t* block = find_fit(size);
    list_remove(block);
    size_t min_size = round_up(2 * sizeof(block_t) + sizeof(size_t), ALIGNMENT);
    //*(block+size)
    if (block != NULL) {
        // SPLITTING
        size_t difference = get_size(block) - size;
        if (difference > min_size) {
            block_t* new_block = (block_t*)((void*)block + size);
            set_header(new_block, difference, false);
            list_add(new_block);
            if (block == mm_heap_last) mm_heap_last = new_block;
            set_header(block, size, true);  // split: use requested size
        } else {
            set_header(block, get_size(block),
                       true);  // no split: keep original size
        }
        return block->payload;
    }
    // printf("could not find block, making a new one");
    // Otherwise, a new block needs to be allocated at the end of the heap
    block = mem_sbrk(size);
    if (block == (void*)-1) {
        return NULL;
    }

    // Update mm_heap_first and mm_heap_last since we extended the heap
    if (mm_heap_first == NULL) {
        mm_heap_first = block;
    }
    mm_heap_last = block;

    // Initialize the block with the allocated size

    set_header(block, size, true);

    return block->payload;
}

/**
 * merge a free block with its neibhors if they are not allocated, returns the
 * new merged block
 */
block_t* merge(block_t* block) {
    bool isBlockLast = false;
    block_t* ret = block;

    if (block != mm_heap_last) {
        // check next block, block will absorb next
        block_t* next = (block_t*)((void*)block + get_size(block));
        if (!is_allocated(next)) {
            if (next == mm_heap_last) isBlockLast = true;
            set_header(block, (get_size(block) + get_size(next)), false);
            // remove next, since it doesn't exist any more
            list_remove(next);
        }
    }

    if (block != mm_heap_first) {
        // check prev, prev will absorb curr block
        block_t* prev = get_prev_block(block);
        if (!is_allocated(prev)) {
            if (block == mm_heap_last) isBlockLast = true;
            set_header(prev, (get_size(prev) + get_size(block)), false);
            list_remove(prev);
            ret = prev;
        }
    }

    if (isBlockLast) mm_heap_last = ret;
    return ret;
}

/**
 * mm_free - Releases a block to be reused for future allocations
 */
void mm_free(void* ptr) {
    // printf("free\n");
    // mm_free(NULL) does nothing
    if (ptr == NULL) {
        return;
    }

    // Mark the block as unallocated
    block_t* block = block_from_payload(ptr);
    set_header(block, get_size(block), false);
    block = merge(block);
    list_add(block);
}
/* mm_realloc - Change the size of the block by mm_mallocing a new block,
 *      copying its data, and mm_freeing the old block.
 */
void* mm_realloc(void* old_ptr, size_t size) {
    // printf("realloc\n");
    (void)old_ptr;
    (void)size;
    if (old_ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(old_ptr);
        return NULL;
    }

    void* new_ptr = mm_malloc(size);
    // printf("new_ptr: %p\n", new_ptr);

    block_t* old_block = block_from_payload(old_ptr);
    size_t old_payload_size = get_size(old_block) - 2 * sizeof(block_t);

    // printf("sizeof old payload: %zu\n", old_payload_size);
    memcpy(new_ptr, old_ptr,
           (size < old_payload_size ? size : old_payload_size));
    mm_free(old_ptr);

    return new_ptr;
}

/**
 * mm_calloc - Allocate the block and set it to zero.
 */
void* mm_calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void* ptr = mm_malloc(total_size);

    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }

    return ptr;
}

/**
 * mm_checkheap - So simple, it doesn't need a checker!
 */
void mm_checkheap(void) {
    //   for (
    //       block_t *curr = mm_heap_first;
    //       mm_heap_last != NULL && curr <= mm_heap_last;
    //       curr = (void *) curr + get_size(curr)
    //   ) {

    //       if (is_allocated(curr)) {
    //           printf("F ");
    //       }else{
    //           printf("E ");
    //       }
    //   }
    //   printf("END\n");
}
