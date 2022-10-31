#include "include/stdlib.h"

// Move to https://wiki.osdev.org/Writing_A_Page_Frame_Allocator

int freeMem = NULL;

void initializeMem() {
    if (freeMem != NULL) return;
    freeMem = FREE_MEM;
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
    int _freeMem = freeMem - sizeof(mem);
    memset(mem, 0, sizeof(mem));
    freeMem = _freeMem;
    return;
}

// void* calloc(size_t size);
// void* realloc(void* mem, size_t size);

test_t MALLOC_TEST(void) {
    char* a = (char*)malloc(20);
    char* b = (char*)malloc(20);
    char* c = (char*)malloc(20);
    char* d = (char*)malloc(20);
    a[0] = '1';
    b[0] = '2';
    c[0] = '3';
    d[0] = '4';
    free(a);
    print_memory();
    printf("%c, %c, %c, %c\n", a[0], b[0], c[0], d[0]);
    return 0;
}