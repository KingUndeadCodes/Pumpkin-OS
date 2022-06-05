#include "include/stdlib.h"

int freeMem;

void initializeMem(){
    freeMem = FREE_MEM;
}

void* kmalloc(size_t size) {
    memset((void*)freeMem, 0, size);
    void* address = (void*)freeMem;
    freeMem += size;
    return address;
}

void kfree(void* mem) {
    freeMem -= sizeof(mem - 5);
    mem = kmalloc(0);
}
