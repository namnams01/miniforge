#include <stdio.h>
#include <stddef.h>   // size_t
#include <stdint.h>   // uintptr_t (for the assert later)
#include <assert.h>
#include <stdlib.h>


static inline size_t align_up(size_t x, size_t a) {
    assert((a & (a - 1)) == 0);   // a must be a power of two
    return (x + (a - 1)) & ~(a - 1);
}

typedef struct {
    unsigned char *base;     // start of the backing block
    size_t         size;     // total capacity
    size_t         offset;   // bump pointer: bytes used so far
} Arena;

void *arena_alloc(Arena *a, size_t n, size_t alignment) {
    size_t p = align_up(a->offset, alignment);
    if (p + n > a->size) return NULL;   // out of room
    a->offset = p + n;
    return a->base + p;
}

void arena_reset(Arena *a) { a->offset = 0; }  // "frees" everything


int main(void) {
    Arena a = { .base = malloc(1024), .size = 2048, .offset = 0 };

    char   *c = arena_alloc(&a, sizeof(char),   1);
    double *d = arena_alloc(&a, sizeof(double), 8);

    printf("char   at %p\n", (void *)c);
    printf("double at %p\n", (void *)d);
    printf("double aligned to 8: %s\n",
           ((uintptr_t)d % 8 == 0) ? "yes" : "no");
    void *too_big = arena_alloc(&a, 2048, 8);

    printf("over-capacity alloc returns NULL: %s\n", too_big == NULL ? "yes" : "no");


    free(a.base);
    return 0;
}