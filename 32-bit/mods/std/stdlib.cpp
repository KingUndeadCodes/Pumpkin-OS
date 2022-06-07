#include "include/stdlib.h"
#include <string.h>
#include <text.h>

int freeMem = FREE_MEM;

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
    int _freeMem = freeMem - sizeof(mem);
    memset(mem, 0, sizeof(mem));
    freeMem = _freeMem;
    return;
}
