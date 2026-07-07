#include "include/stdlib.h"
#include "../../dev/serial/serial.h"

// Divide Functions
div_t div(int numerator, int denominator) {
    div_t div;
    div.quot = numerator / denominator;
    div.rem = numerator % denominator;
    return div;
}

__attribute__((__noreturn__)) void abort(void) {
	serial_write_string("kernel: panic: abort()\n");
    asm volatile("hlt");
	while (1);
	__builtin_unreachable();
}

// Define the memory region for dynamic allocation
static uint8_t memory_pool[POOL_SIZE];

// Define a linked list of free blocks in the memory pool
typedef struct _block_t {
    size_t size;
    struct _block_t *next;
} block_t;

static block_t *free_list = NULL;

size_t get_kmalloc_free_bytes(void) {
    unsigned long flags = enter_critical();
    size_t total_free = 0;
    block_t *block = free_list;
    while (block != NULL) {
        total_free += block->size;
        block = block->next;
    }
    exit_critical(flags);
    return total_free;
}

// Initialize the free list with the entire memory pool
void initialize_memory_pool(void) {
    free_list = (block_t *)memory_pool;
    free_list->size = sizeof(memory_pool) - sizeof(block_t);
    free_list->next = NULL;
}

// Split a block into two parts: one of size 'size', and the remainder
block_t *split_block(block_t *block, size_t size) {
    block_t *new_block = (block_t *)((uint8_t *)block + sizeof(block_t) + size);
    new_block->size = block->size - sizeof(block_t) - size;
    new_block->next = block->next;
    block->size = size;
    block->next = new_block;
    return block;
}

void *malloc(size_t size) {
    unsigned long flags = enter_critical();

    // Find the first free block that is large enough
    block_t *prev_block = NULL;
    block_t *block = free_list;
    while (block != NULL && block->size < size) {
        prev_block = block;
        block = block->next;
    }

    // If no free block is large enough, return NULL
    if (block == NULL) {
        exit_critical(flags);
        return NULL;
    }

    // If the free block is larger than necessary, split it
    if (block->size > size + sizeof(block_t)) {
        block = split_block(block, size);
    }

    // Remove the block from the free list
    if (prev_block == NULL) {
        free_list = block->next;
    } else {
        prev_block->next = block->next;
    }

    exit_critical(flags);
    return (void *)((uint8_t *)block + sizeof(block_t));
}

void free(void *ptr) {
    unsigned long flags = enter_critical();
    // Add the block to the free list
    block_t *block = (block_t *)((uint8_t *)ptr - sizeof(block_t));
    block->next = free_list;
    free_list = block;
    exit_critical(flags);
}

void *calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void *ptr = malloc(total_size);
    if (ptr == NULL) {
        return NULL;
    }
    memset(ptr, 0, total_size);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }

    block_t *block = (block_t *)((uint8_t *)ptr - sizeof(block_t));
    if (size <= block->size) {
        if (block->size - size >= sizeof(block_t)) {
            block = split_block(block, size);
        }
        return ptr;
    }

    void *new_ptr = malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}