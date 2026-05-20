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
    if(free_list_head) free_list_head->prev = block;
    free_list_head = block;
}

void list_remove(block_t* block) {
    block_t* curr = free_list_head;
    if (!curr || !block) return;
    while (curr && curr != block) {
        curr = curr->next;
    }
    //if we've reached the end of the list, the block isn't free
    if(!curr) return;
    if (curr->prev){
        curr->prev->next = curr->next;
        
    }else /*curr is head*/
        free_list_head = curr->next;

    if(curr->next){
        curr->next->prev = curr->prev;
    }
}

/** Rounds up `size` to the nearest multiple of `n` */
static size_t round_up(size_t size, size_t n) {
    return (size + (n - 1)) / n * n;
}

/** Set's a block's header with the given size and allocation state */
static void set_header(block_t* block, size_t size, bool is_allocated) {
    block->header = size | is_allocated;
}

/** Extracts a block's size from its header */
static size_t get_size(block_t* block) { return block->header & ~1; }

/** Extracts a block's allocation state from its header */
static bool is_allocated(block_t* block) { return block->header & 1; }

/**
 * Finds the first free block in the heap with at least the given size.
 * If no block is large enough, returns NULL.
 */
static block_t* find_fit(size_t size) {
    // Traverse the blocks in the heap using the implicit list
    for (block_t* curr = free_list_head; 
        curr ;
        curr = curr->next) {
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

void coalesce() {
    if (mm_heap_first == NULL || mm_heap_last == NULL) {
        return;
    }

    for (block_t* curr = mm_heap_first; curr <= mm_heap_last;
         curr = (block_t*)((char*)curr + get_size(curr))) {
        // Skip allocated blocks and isolated free blocks
        block_t* next = (block_t*)((char*)curr + get_size(curr));
        if (is_allocated(curr) || next > mm_heap_last || is_allocated(next)) {
            continue;
        }

        // Absorb all consecutive free blocks that follow into curr
        while (next <= mm_heap_last && !is_allocated(next)) {
            size_t new_size = get_size(curr) + get_size(next);
            set_header(curr, new_size, false);

            if (mm_heap_last == next) {
                mm_heap_last = curr;
            }

            next = (block_t*)((char*)curr + get_size(curr));
        }
    }
}
static bool called_list_remove = false;
/**
 * mm_malloc - Allocates a block with the given size
 */
void* mm_malloc(size_t size) {
    // The block must have enough space for a header and be 16-byte aligned
    size = round_up(sizeof(block_t) + size, ALIGNMENT);

    // printf("Size of block_t: %zu \n", sizeof(block_t));
    //   coalesce();
    // If there is a large enough free block, use it
    block_t* block = find_fit(size);
    list_remove(block);
    size_t min_size = round_up(sizeof(block_t)+sizeof(size_t), ALIGNMENT);
    //*(block+size)
    if (block != NULL) {
        // printf("found block at: %p, size = %zu\n", block, get_size(block));
        size_t difference = get_size(block) - size;
        if (difference > min_size) {
            block_t* new_block = (block_t*)((void*)block + size);

            set_header(new_block, difference, false);

            list_add(new_block);

            if (block == mm_heap_last) mm_heap_last = new_block;

        }
        // size_t old_block_size = difference > ALIGNMENT? size:
        // get_size(block);

        set_header(block, size, true);
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
 * mm_free - Releases a block to be reused for future allocations
 */
void mm_free(void* ptr) {
    // mm_free(NULL) does nothing
    if (ptr == NULL) {
        return;
    }

    // Mark the block as unallocated
    block_t* block = block_from_payload(ptr);
    set_header(block, get_size(block), false);

    list_add(block);
} 
 /* mm_realloc - Change the size of the block by mm_mallocing a new block,
 *      copying its data, and mm_freeing the old block.
 */
void* mm_realloc(void* old_ptr, size_t size) {
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
    size_t old_payload_size = get_size(old_block) - sizeof(block_t);
    
    // printf("sizeof old payload: %zu\n", old_payload_size);
    memcpy(new_ptr, old_ptr, (size < old_payload_size ? size : old_payload_size));
    mm_free(old_ptr);

    return new_ptr;
}

/**
 * mm_calloc - Allocate the block and set it to zero.
 */
void* mm_calloc(size_t nmemb, size_t size) {
    (void)nmemb;
    (void)size;

    void* ptr = mm_malloc(size);

    memset(ptr, 0, size);

    return ptr;
}

/**
 * mm_checkheap - So simple, it doesn't need a checker!
 */
void mm_checkheap(void) {
    if (called_list_remove) {
        printf("called list_remove in malloc");
    } else {
        printf("list_remove not called");
    }

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
