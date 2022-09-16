#include "include/stdlib.h"

int freeMem = FREE_MEM;

void initializeMem() {
    freeMem = FREE_MEM;
}

int getFreeMem() {
    return freeMem;
}

void print_memory(void) {
    print("[DEBUG] ", COLOR_YELLOW);
    print("Memory: ", COLOR_BLUE);
    print(itoa(freeMem, 10));
    print("\n");
}

void* malloc(size_t size) {
    memset((void*)freeMem, 0, size);
    void* address = (void*)freeMem;
    freeMem += size;
    return address;
}

void free(void* mem) {
    int _freeMem = freeMem - (freeMem - (int)mem);
    memset(mem, 0, sizeof(mem));
    freeMem = _freeMem;
    return;
}

// void* calloc(size_t size);
// void* realloc(void* mem, size_t size);