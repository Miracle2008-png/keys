/* Real C: pointers, structs, macros, preprocessor. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 256
#define UNUSED(x) ((void)(x))

typedef struct Node {
    char  *key;
    int    value;
    struct Node *next;
} Node;

static Node *node_create(const char *key, int value) {
    Node *n = calloc(1, sizeof(Node));
    if (!n) { return NULL; }
    n->key = strdup(key);
    n->value = value;
    return n;
}

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);
    Node *head = node_create("answer", 42);
    printf("%s = %d\n", head->key, head->value);
    free(head->key);
    free(head);
    return EXIT_SUCCESS;
}
