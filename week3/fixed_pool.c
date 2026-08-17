#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    unsigned char *base;
    size_t size;
    size_t offset;
} Arena;

typedef struct FreeNode {
    struct FreeNode *next;
} FreeNode;

typedef struct {
    size_t generation;
    size_t refcount;
    bool allocated;
} BlockMeta;

typedef struct {
    Arena arena;
    size_t block_size;
    size_t stride;
    size_t block_count;
    size_t free_count;
    FreeNode *free_head;
    BlockMeta *metadata;
} FixedPool;

bool arena_init(Arena *arena, size_t capacity) {
    if (arena == NULL) {
        return false;
    }

    arena->base = NULL;
    arena->size = 0;
    arena->offset = 0;

    if (capacity == 0) {
        return false;
    }

    arena->base = malloc(capacity);
    if (arena->base == NULL) {
        return false;
    }

    arena->size = capacity;
    return true;
}

void arena_destroy(Arena *arena) {
    if (arena == NULL) {
        return;
    }

    free(arena->base);
    arena->base = NULL;
    arena->size = 0;
    arena->offset = 0;
}

bool fixed_pool_init(FixedPool *pool, size_t capacity, size_t block_size) {
    if (pool == NULL) {
        return false;
    }
    if (capacity == 0) {
        return false;
    }
    if (block_size == 0) {
        return false;
    }

    size_t minimum_size;
    if (block_size > sizeof(FreeNode)) {
        minimum_size = block_size;
    } else {
        minimum_size = sizeof(FreeNode);
    }

    size_t alignment = _Alignof(max_align_t);
    size_t remainder = minimum_size % alignment;
    size_t stride;
    size_t padding = alignment - remainder;

    if (minimum_size > SIZE_MAX - padding) {
        return false;
    }

    if (remainder == 0) {
        stride = minimum_size;
    } else {
        stride = minimum_size + alignment - remainder;
    }

    size_t block_count = capacity / stride;
    if (block_count == 0) {
        return false;
    }

    *pool = (FixedPool){0};

    if (!arena_init(&pool->arena, capacity)) {
        return false;
    }

    pool->block_size = block_size;
    pool->stride = stride;
    pool->block_count = block_count;
    pool->free_count = block_count;
    pool->arena.offset = block_count * stride;

    pool->metadata = calloc(block_count, sizeof *pool->metadata);
    if (pool->metadata == NULL) {
        arena_destroy(&pool->arena);
        *pool = (FixedPool){0};
        return false;
    }

    for (size_t i = 0; i < block_count - 1; i++) {
        FreeNode *current =
            (FreeNode *)(pool->arena.base + i * pool->stride);
        FreeNode *next_node =
            (FreeNode *)(pool->arena.base + (i + 1) * pool->stride);
        current->next = next_node;
    }

    FreeNode *last =
        (FreeNode *)(pool->arena.base + (block_count - 1) * pool->stride);
    last->next = NULL;
    pool->free_head = (FreeNode *)pool->arena.base;

    return true;
}

void fixed_pool_destroy(FixedPool *pool) {
    if (pool == NULL) {
        return;
    }

    free(pool->metadata);
    arena_destroy(&pool->arena);
    *pool = (FixedPool){0};
}

void *fixed_pool_alloc(FixedPool *pool) {
    if (pool == NULL) {
        return NULL;
    }
    if (pool->free_head == NULL) {
        return NULL;
    }

    FreeNode *allocated = pool->free_head;
    pool->free_head = allocated->next;
    pool->free_count--;

    size_t offset = (unsigned char *)allocated - pool->arena.base;
    size_t index = offset / pool->stride;

    pool->metadata[index].allocated = true;
    pool->metadata[index].generation++;
    pool->metadata[index].refcount = 1;

    return allocated;
}

bool fixed_pool_free(FixedPool *pool, void *block) {
    if (pool == NULL) {
        return false;
    }
    if (block == NULL) {
        return false;
    }

    uintptr_t address = (uintptr_t)block;
    uintptr_t base = (uintptr_t)pool->arena.base;

    if (address < base) {
        return false;
    }

    size_t offset = address - base;
    size_t used_bytes = pool->block_count * pool->stride;

    if (offset >= used_bytes) {
        return false;
    }
    if (offset % pool->stride != 0) {
        return false;
    }

    size_t index = offset / pool->stride;
    if (!pool->metadata[index].allocated) {
        return false;
    }

    FreeNode *node = (FreeNode *)block;
    node->next = pool->free_head;
    pool->free_head = node;

    pool->metadata[index].allocated = false;
    pool->metadata[index].refcount = 0;
    pool->free_count++;

    return true;
}

static bool init_test_pool(FixedPool *pool) {
    return fixed_pool_init(pool, 4096, 256);
}

static size_t count_free_nodes(const FixedPool *pool) {
    size_t count = 0;

    for (const FreeNode *node = pool->free_head;
         node != NULL;
         node = node->next) {
        count++;

        if (count > pool->block_count) {
            break;
        }
    }

    return count;
}

static bool test_initial_free_list(void) {
    FixedPool pool = {0};
    if (!init_test_pool(&pool)) {
        return false;
    }

    bool passed =
        pool.block_count == 16 &&
        pool.free_count == 16 &&
        count_free_nodes(&pool) == 16;

    fixed_pool_destroy(&pool);
    return passed;
}

static bool test_allocation_metadata(void) {
    FixedPool pool = {0};
    if (!init_test_pool(&pool)) {
        return false;
    }

    void *block = fixed_pool_alloc(&pool);
    bool passed = false;

    if (block != NULL) {
        size_t offset = (unsigned char *)block - pool.arena.base;
        size_t index = offset / pool.stride;

        passed =
            index == 0 &&
            pool.free_count == 15 &&
            count_free_nodes(&pool) == 15 &&
            pool.metadata[index].allocated &&
            pool.metadata[index].generation == 1 &&
            pool.metadata[index].refcount == 1;
    }

    fixed_pool_destroy(&pool);
    return passed;
}

static bool test_allocate_free_reallocate(void) {
    FixedPool pool = {0};
    if (!init_test_pool(&pool)) {
        return false;
    }

    void *first = fixed_pool_alloc(&pool);
    if (first == NULL) {
        fixed_pool_destroy(&pool);
        return false;
    }

    size_t index =
        ((unsigned char *)first - pool.arena.base) / pool.stride;
    size_t first_generation = pool.metadata[index].generation;

    bool first_free_succeeded = fixed_pool_free(&pool, first);
    bool metadata_was_reset =
        !pool.metadata[index].allocated &&
        pool.metadata[index].refcount == 0 &&
        pool.free_count == pool.block_count;
    bool double_free_rejected = !fixed_pool_free(&pool, first);

    void *second = fixed_pool_alloc(&pool);
    bool passed =
        first_free_succeeded &&
        metadata_was_reset &&
        double_free_rejected &&
        second == first &&
        pool.free_count == pool.block_count - 1 &&
        pool.metadata[index].allocated &&
        pool.metadata[index].refcount == 1 &&
        pool.metadata[index].generation == first_generation + 1;

    fixed_pool_destroy(&pool);
    return passed;
}

static bool test_exhaustion_uniqueness_recovery(void) {
    FixedPool pool = {0};
    if (!init_test_pool(&pool)) {
        return false;
    }

    void *blocks[16] = {0};
    bool passed = pool.block_count == 16;

    for (size_t i = 0; passed && i < pool.block_count; i++) {
        blocks[i] = fixed_pool_alloc(&pool);
        if (blocks[i] == NULL) {
            passed = false;
        }
    }

    if (passed) {
        passed = pool.free_count == 0 && fixed_pool_alloc(&pool) == NULL;
    }

    for (size_t i = 0; passed && i < pool.block_count; i++) {
        for (size_t j = i + 1; j < pool.block_count; j++) {
            if (blocks[i] == blocks[j]) {
                passed = false;
                break;
            }
        }
    }

    for (size_t i = 0; i < pool.block_count; i++) {
        if (blocks[i] != NULL && !fixed_pool_free(&pool, blocks[i])) {
            passed = false;
        }
    }

    passed =
        passed &&
        pool.free_count == pool.block_count &&
        count_free_nodes(&pool) == pool.block_count;

    fixed_pool_destroy(&pool);
    return passed;
}

static bool test_null_block(void) {
    FixedPool pool = {0};
    if (!init_test_pool(&pool)) {
        return false;
    }

    size_t before = pool.free_count;
    bool passed =
        !fixed_pool_free(&pool, NULL) && pool.free_count == before;

    fixed_pool_destroy(&pool);
    return passed;
}

static bool test_null_pool(void) {
    int foreign = 0;
    return !fixed_pool_free(NULL, &foreign);
}

static bool test_interior_pointer(void) {
    FixedPool pool = {0};
    if (!init_test_pool(&pool)) {
        return false;
    }

    void *block = fixed_pool_alloc(&pool);
    bool passed = block != NULL;

    if (passed) {
        size_t before = pool.free_count;
        bool interior_rejected =
            !fixed_pool_free(&pool, (unsigned char *)block + 1);
        bool unchanged = pool.free_count == before;
        bool real_free_succeeded = fixed_pool_free(&pool, block);

        passed = interior_rejected && unchanged && real_free_succeeded;
    }

    fixed_pool_destroy(&pool);
    return passed;
}

static bool test_one_past_pointer(void) {
    FixedPool pool = {0};
    if (!init_test_pool(&pool)) {
        return false;
    }

    unsigned char *one_past =
        pool.arena.base + pool.block_count * pool.stride;
    size_t before = pool.free_count;
    bool passed =
        !fixed_pool_free(&pool, one_past) && pool.free_count == before;

    fixed_pool_destroy(&pool);
    return passed;
}

static bool test_foreign_pointer(void) {
    FixedPool pool = {0};
    if (!init_test_pool(&pool)) {
        return false;
    }

    void *foreign = malloc(pool.stride);
    if (foreign == NULL) {
        fixed_pool_destroy(&pool);
        return false;
    }

    size_t before = pool.free_count;
    bool passed =
        !fixed_pool_free(&pool, foreign) && pool.free_count == before;

    free(foreign);
    fixed_pool_destroy(&pool);
    return passed;
}

static bool test_double_free(void) {
    FixedPool pool = {0};
    if (!init_test_pool(&pool)) {
        return false;
    }

    void *block = fixed_pool_alloc(&pool);
    bool passed = block != NULL;

    if (passed) {
        bool first_free_succeeded = fixed_pool_free(&pool, block);
        size_t after_first_free = pool.free_count;
        bool second_free_rejected = !fixed_pool_free(&pool, block);

        passed =
            first_free_succeeded &&
            second_free_rejected &&
            pool.free_count == after_first_free;
    }

    fixed_pool_destroy(&pool);
    return passed;
}

typedef bool (*TestFn)(void);

typedef struct {
    const char *name;
    TestFn run;
} TestCase;

int main(void) {
    const TestCase tests[] = {
        {"initial free list", test_initial_free_list},
        {"allocation metadata", test_allocation_metadata},
        {"allocate/free/reallocate", test_allocate_free_reallocate},
        {"exhaustion/uniqueness/recovery",
         test_exhaustion_uniqueness_recovery},
        {"NULL block", test_null_block},
        {"NULL pool", test_null_pool},
        {"interior pointer", test_interior_pointer},
        {"one-past pointer", test_one_past_pointer},
        {"foreign pointer", test_foreign_pointer},
        {"double free", test_double_free},
    };

    const size_t test_count = sizeof tests / sizeof tests[0];
    size_t passed = 0;

    for (size_t i = 0; i < test_count; i++) {
        bool result = tests[i].run();

        printf("[%s] %s\n", result ? "PASS" : "FAIL", tests[i].name);
        if (result) {
            passed++;
        }
    }

    printf("\n%zu/%zu tests passed\n", passed, test_count);
    return passed == test_count ? EXIT_SUCCESS : EXIT_FAILURE;
}
