#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define ALIGNMENT 16
#define BLOCK_SIZE sizeof(MemoryBlock)

// Structure to represent a memory block in the custom memory manager
typedef struct MemoryBlock {
    size_t size;           // Size of the memory block
    struct MemoryBlock *next;  // Pointer to the next memory block
    int free;              // Flag to indicate if the block is free
} MemoryBlock;

// Function declarations
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t num, size_t size);
void *realloc(void *ptr, size_t newSize);
void print_memory_map(void);
MemoryBlock *find_free_block(MemoryBlock **last, size_t size);
MemoryBlock *request_space(MemoryBlock* last, size_t size);
void merge_free_blocks(MemoryBlock *block);

// Global variable to keep track of the memory blocks
MemoryBlock *free_list = NULL;

// Main function to demonstrate the usage of custom memory management functions
int main() {
    printf("Initial memory map:\n");
    print_memory_map();

    // Dynamic memory allocation using custom malloc function
    int *array = (int*)malloc(40 * sizeof(int));
    printf("Memory map after malloc for array:\n");
    print_memory_map();

    // Allocating memory using custom calloc function
    int *array_calloc = (int*)calloc(25, sizeof(int));
    printf("Memory map after calloc for array_calloc:\n");
    print_memory_map();

    // Reallocating memory using custom realloc function
    array = (int*)realloc(array, 20 * sizeof(int));
    printf("Memory map after realloc for array:\n");
    print_memory_map();

    // Freeing memory using custom free function
    free(array);
    printf("Memory map after free for array:\n");
    print_memory_map();

    // Freeing memory allocated by calloc
    free(array_calloc);
    printf("Memory map after free for array_calloc:\n");
    print_memory_map();

    return 0;
}

// Function definitions below...

// Merges consecutive free blocks into a single larger free block.
// This helps to reduce fragmentation.
void merge_free_blocks(MemoryBlock *block) {
    while (block->next != NULL && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
    }
}

// Finds a free block that can accommodate a memory request of 'size' bytes.
MemoryBlock *find_free_block(MemoryBlock **last, size_t size) {
    MemoryBlock *current = *last;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
}

// Requests a new block of memory from the system using sbrk.
MemoryBlock *request_space(MemoryBlock* last, size_t size) {
    MemoryBlock *block = sbrk(0);
    void *request = sbrk(size + BLOCK_SIZE);
    if (request == (void*) -1) {
        return NULL; // sbrk failed
    }

    if (last) {
        last->next = block;
    }

    block->size = size;
    block->next = NULL;
    block->free = 0;
    return block;
}

// Custom implementation of malloc to allocate memory.
void *malloc(size_t size) {
    if (size <= 0) {
        return NULL;
    }

    size_t alignedSize = (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    MemoryBlock *block;

    if (!free_list) {
        block = request_space(NULL, alignedSize);
        if (!block) {
            return NULL;
        }
        free_list = block;
    } else {
        MemoryBlock *last = free_list;
        block = find_free_block(&last, alignedSize);
        if (!block) {
            block = request_space(last, alignedSize);
            if (!block) {
                return NULL;
            }
        } else {
            if (block->size > alignedSize + BLOCK_SIZE) {
                // Splitting the block if it's larger than needed
                MemoryBlock *newBlock = (MemoryBlock *)((char *)block + BLOCK_SIZE + alignedSize);
                newBlock->size = block->size - alignedSize - BLOCK_SIZE;
                newBlock->free = 1;
                newBlock->next = block->next;

                block->size = alignedSize;
                block->next = newBlock;
            }
            block->free = 0;
        }
    }

    return (block + 1);
}

// Custom implementation of free to release allocated memory.
void free(void *ptr) {
    if (!ptr) {
        return;
    }

    MemoryBlock *block = (MemoryBlock*)ptr - 1;
    block->free = 1;
    merge_free_blocks(block);
}

// Custom implementation of calloc using malloc and memset.
void *calloc(size_t num, size_t size) {
    size_t totalSize = num * size;
    void *ptr = malloc(totalSize);
    if (ptr) {
        memset(ptr, 0, totalSize);
    }
    return ptr;
}

// Custom implementation of realloc to resize allocated memory.
void *realloc(void *ptr, size_t newSize) {
    if (!ptr) {
        // Equivalent to malloc if the original pointer is NULL
        return malloc(newSize);
    }

    MemoryBlock *block = (MemoryBlock *)((char *)ptr - BLOCK_SIZE);
    if (block->size >= newSize) {
        // Check if the block can be split
        if (block->size >= newSize + BLOCK_SIZE + ALIGNMENT) {
            // Split the block
            MemoryBlock *newBlock = (MemoryBlock *)((char *)block + BLOCK_SIZE + newSize);
            newBlock->size = block->size - newSize - BLOCK_SIZE;
            newBlock->free = 1;
            newBlock->next = block->next;

            block->size = newSize;
            block->next = newBlock;
            merge_free_blocks(newBlock); // Merge with next if it is also free
        }
        return ptr;
    }

    // Allocate a new memory block and copy the old data
    void *newPtr = malloc(newSize);
    if (newPtr) {
        memcpy(newPtr, ptr, block->size);
        free(ptr);
    }
    return newPtr;
}

// Prints the current memory block layout.
void print_memory_map() {
    MemoryBlock *current = free_list;
    printf("Memory Blocks:\n");
    while (current != NULL) {
        printf("Block at %p, size: %zu, free: %d\n", 
               (void*)current, current->size, current->free);
        current = current->next;
    }
}
