/*
 * mm-implicit.c - The best malloc package EVAR!
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"

/** The required alignment of heap payloads */
const size_t ALIGNMENT = 2 * sizeof(size_t);

/** Minimum block size: Header (8) + Next (8) + Prev (8) + Footer (8) = 32 bytes */
#define MIN_BLOCK_SIZE 32

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
    if(!curr) return;
    if (curr->prev){
        curr->prev->next = curr->next;
    } else {
        free_list_head = curr->next;
    }
    if(curr->next){
        curr->next->prev = curr->prev;
    }
}

/** Rounds up `size` to the nearest multiple of `n` */
static size_t round_up(size_t size, size_t n) {
    return (size + (n - 1)) / n * n;
}

/** Extracts a block's size from its header */
static size_t get_size(block_t* block) { return block->header & ~1; }

/** Extracts a block's allocation state from its header */
static bool is_allocated(block_t* block) { return block->header & 1; }

/* --- Footer and Prev Block Helpers --- */
static size_t* get_footer(block_t *block) {
    return (size_t*)((char*)block + get_size(block) - sizeof(size_t));
}

static void set_footer(block_t *block) {
    *get_footer(block) = block->header;
}

static block_t* get_prev_block(block_t *block) {
    size_t prev_footer = *((size_t*)((char*)block - sizeof(size_t)));
    size_t prev_size = prev_footer & ~1;
    return (block_t*)((char*)block - prev_size);
}

/** Sets a block's header and footer with the given size and allocation state */
static void set_header(block_t* block, size_t size, bool is_allocated) {
    block->header = size | is_allocated;
    set_footer(block);
}

/** Finds the first free block in the heap with at least the given size. */
static block_t* find_fit(size_t size) {
    for (block_t* curr = free_list_head; curr; curr = curr->next) {
        if (get_size(curr) >= size) {
            return curr;
        }
    }
    return NULL;
}

/** Gets the block corresponding to a given payload pointer */
static block_t* block_from_payload(void* ptr) {
    return (block_t*)((char*)ptr - offsetof(block_t, payload));
}

bool mm_init(void) {
    void* padding = mem_sbrk((3 * ALIGNMENT) - sizeof(block_t));
    if (padding == (void*)-1) {
        return false;
    }

    mm_heap_first = NULL;
    mm_heap_last = NULL;
    free_list_head = NULL;
    return true;
}


void* mm_malloc(size_t size) {
    if (size == 0) return NULL;

    // Account for header, footer, and alignment constraints
    size = round_up(2 * sizeof(size_t) + size, ALIGNMENT);
    if (size < MIN_BLOCK_SIZE) {
        size = MIN_BLOCK_SIZE;
    }

    block_t* block = find_fit(size);
    
    if (block != NULL) {
        list_remove(block);
        size_t difference = get_size(block) - size;
        
        // Splitting
        if (difference >= MIN_BLOCK_SIZE) {
            block_t* new_block = (block_t*)((char*)block + size);
            
            // Set header/footer for the split blocks
            set_header(block, size, true);
            set_header(new_block, difference, false);
            
            list_add(new_block);

            if (block == mm_heap_last) {
                mm_heap_last = new_block;
            }
        } else {
            set_header(block, get_size(block), true);
        }
        return block->payload;
    }

    block = mem_sbrk(size);
    if (block == (void*)-1) {
        return NULL;
    }

    if (mm_heap_first == NULL) {
        mm_heap_first = block;
    }
    mm_heap_last = block;

    set_header(block, size, true);
    return block->payload;
}

void mm_free(void* ptr) {
    if (ptr == NULL) return;

    block_t* block = block_from_payload(ptr);
    size_t size = get_size(block);
    bool is_last = (block == mm_heap_last);

    // Coalesce Next
    block_t* next = (block_t*)((char*)block + size);
    if (block < mm_heap_last && !is_allocated(next)) {
        if (next == mm_heap_last) {
            is_last = true;
        }
        list_remove(next);
        size += get_size(next);
    }

    // Coalesce Previous
    if (block > mm_heap_first) {
        block_t* prev = get_prev_block(block);
        if (!is_allocated(prev)) {
            list_remove(prev);
            size += get_size(prev);
            block = prev;
        }
    }

    set_header(block, size, false);
    
    if (is_last) {
        mm_heap_last = block;
    }

    list_add(block);
} 

void* mm_realloc(void* old_ptr, size_t size) {
    if (old_ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(old_ptr);
        return NULL;
    }

    void* new_ptr = mm_malloc(size);
    if (!new_ptr) return NULL;
    
    block_t* old_block = block_from_payload(old_ptr);
    // Payload calculation accounts for both header and footer
    size_t old_payload_size = get_size(old_block) - 2 * sizeof(size_t);
    
    memcpy(new_ptr, old_ptr, (size < old_payload_size ? size : old_payload_size));
    mm_free(old_ptr);

    return new_ptr;
}

void* mm_calloc(size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void* ptr = mm_malloc(bytes);
    if (ptr) {
        memset(ptr, 0, bytes);
    }
    return ptr;
}

void mm_checkheap(void) {

}