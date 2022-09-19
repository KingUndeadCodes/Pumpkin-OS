#include "fs.h"

uint32_t previous_node = 0;
struct Node* Nodes;

void destroy_fs(void) {
    memset(&Nodes, 0, sizeof(Nodes));
    previous_node = 0;
    return;
}

struct Node init_node(uint32_t* previous) {
    struct Node n;
    n.n_data[0] = 0;
    n.n_index = *(previous + 1);
    n.n_prev = *(previous == 0 ? NULL : previous);
    (void) *previous++;
    return n;
}

void init_file(struct File F) {
    struct Node n = init_node(&previous_node);
    char* t;
    for (t = F.f_name; *t != '\0'; t++) n.n_data[t - F.f_name] = *t;
    Nodes = &n;
    (void) *Nodes++;
    return;
}

char* read_file(struct File F) {
    struct Node* nodes = Nodes;
    size_t d = 0;
    for (int n = 0; n < sizeof(*Nodes) / sizeof(struct Node); n++) {
        for (int o = 0; o < strlen(F.f_name); o++) {
            if (nodes[n - 1].n_data[o] == F.f_name[o]) {
                d++;
                continue;
            } 
            break;
        }
        if (d == strlen(F.f_name)) return nodes[n - 1].n_data + 255;
    }
    return NULL;
}

void write_file(struct File F, const char* data) {
    struct Node* nodes = Nodes;
    size_t d = 0;
    for (int n = 0; n < sizeof(*Nodes) / sizeof(struct Node); n++) {
        for (int o = 0; o < strlen(F.f_name); o++) { 
            if (nodes[n - 1].n_data[o] == F.f_name[o]) {
                d++;
                continue;
            }
            break;
        }
        if (d == strlen(F.f_name)) {
            for (int x = 0; x < strlen(data) + 1; x++) nodes[n - 1].n_data[255 + x] = data[x];
            nodes[n - 1].n_data[255 + strlen(data) + 1] = '\0';
            return;
        }
    }
    return;
}

test_t FS_TEST() {
    struct File F;
    strcpy(F.f_name, "hello.txt");
    init_file(F);
    write_file(F, "Hello, World!");
    printf("File contents: %s", read_file(F));
}