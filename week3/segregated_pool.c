#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIGNMENT _Alignof(max_align_t)
#define min_block_size \
    (sizeof(BlockHeader)+sizeof(FreeLinks)+sizeof(BlockFooter))

enum {
    SIZE_CLASS_COUNT = 8
};

typedef struct{
    size_t size;
    bool allocated;
} BlockFooter;

typedef struct BlockHeader {
    size_t size;
    bool allocated;
} BlockHeader;

typedef struct FreeLinks {
    BlockHeader *prev;
    BlockHeader *next;
} FreeLinks;

typedef struct{
    unsigned char *arena;
    size_t arena_size;
    BlockHeader *free_lists[SIZE_CLASS_COUNT];
} SegPool;


static FreeLinks *block_links(BlockHeader *header) {
    unsigned char *address = (unsigned char*)header;
    return (FreeLinks *)(sizeof(*header)+address); 
}

static BlockFooter *block_footer(BlockHeader *header) {
    unsigned char *start = (unsigned char*)header;
    return (BlockFooter *)(start+header->size - sizeof(BlockFooter));
}

static bool align_up(size_t value, size_t alignment, size_t *result){
    if (result == NULL || alignment == 0){
        return false;
    }

    size_t remainder = value%alignment;

    if (remainder == 0){
        *result = value;
        return true;
    }

    size_t padding = alignment-remainder;

    if (value > SIZE_MAX-padding){
        return false;
    }

    *result = value+padding;
    return true;
} 

typedef struct {
    size_t value;
    size_t alignment;
    bool expected_success;
    size_t expected_result; 
} AlignCase;

static size_t size_class_index(size_t size) {
    size_t limit = 64;

    for (size_t index = 0;
         index < SIZE_CLASS_COUNT - 1;
         index++) {

            if (size <= limit){
                return index;
            } else{
                limit *= 2;
            }

    }

    return SIZE_CLASS_COUNT - 1;    
}

typedef struct {
    size_t size;
    size_t expected_class;
} SizeClassCase;

static void free_list_insert(SegPool *pool, BlockHeader *block) {
    size_t class_index = size_class_index(block->size);
    BlockHeader *old_head = pool->free_lists[class_index];
    FreeLinks *links = block_links(block);

    links->prev = NULL;
    links->next = old_head;

    if (old_head != NULL) {
        block_links(old_head)->prev = block;
    }

    pool->free_lists[class_index] = block;
}

static void free_list_remove(SegPool *pool, BlockHeader *block) {
    size_t class_index = size_class_index(block->size);
    FreeLinks *links = block_links(block);

    BlockHeader *prev = links->prev;
    BlockHeader *next = links->next;

    if (prev != NULL) {
        block_links(prev)->next = next;
    } else {
        pool->free_lists[class_index] = next;
    }

    if (next != NULL) {
        block_links(next)->prev = prev;
    }

    links->prev = NULL;
    links->next = NULL;
}

static bool test_size_class_index(void) {
    SizeClassCase cases[] = {
        {48,       0},
        {64,       0},
        {65,       1},
        {128,      1},
        {129,      2},
        {256,      2},
        {257,      3},
        {512,      3},
        {513,      4},
        {1024,     4},
        {1025,     5},
        {2048,     5},
        {2049,     6},
        {4096,     6},
        {4097,     7},
        {SIZE_MAX, 7},
    };

    size_t case_count = sizeof cases / sizeof cases[0];

    for (size_t i = 0; i < case_count; i++) {
        size_t actual = size_class_index(cases[i].size);

        if (actual != cases[i].expected_class) {
            printf(
                "case %zu: size %zu expected class %zu, got %zu\n",
                i,
                cases[i].size,
                cases[i].expected_class,
                actual
            );
            return false;
        }
    }

    return true;
}

static bool test_align_up(void){
        AlignCase cases[] = {
        {0,        16, true,  0},
        {1,        16, true, 16},
        {15,       16, true, 16},
        {16,       16, true, 16},
        {17,       16, true, 32},
        {31,       16, true, 32},
        {32,       16, true, 32},
        {SIZE_MAX, 16, false, 0},
        {7,         0, false, 0},
    };

    size_t case_count = sizeof cases / sizeof cases[0];

    for (size_t i = 0; i < case_count; i++) {
        size_t actual = 0;

        bool success = align_up(
            cases[i].value,
            cases[i].alignment,
            &actual
        );

        if (success != cases[i].expected_success){
            return false;
        }

        if (success && actual != cases[i].expected_result) {
            printf("case %zu: expected %zu, got %zu\n",
           i,
           cases[i].expected_result,
           actual);
           return false;
        }
    }

    if (align_up(7, 16, NULL)) {
        printf("NULL result pointer was incorrectly accepted\n");
        return false;
    }

    return true;
}

static bool test_block_layout(void) {
    _Alignas(max_align_t) unsigned char storage[256] = {0};

    BlockHeader *header = (BlockHeader *)storage;
    header->size = sizeof storage;
    header->allocated = false;

    FreeLinks *links = block_links(header);
    BlockFooter *footer = block_footer(header);

    size_t links_offset =
        (unsigned char *)links - storage;

    size_t footer_offset =
        (unsigned char *)footer - storage;

    size_t footer_end_offset =
        (unsigned char *)(footer + 1) - storage;

    if (links_offset != sizeof(BlockHeader)) {
        printf("links: expected offset %zu, got %zu\n",
               sizeof(BlockHeader),
               links_offset);
        return false;
    }

    if (footer_offset != sizeof storage - sizeof(BlockFooter)) {
        printf("footer: expected offset %zu, got %zu\n",
               sizeof storage - sizeof(BlockFooter),
               footer_offset);
        return false;
    }

    if (footer_end_offset != sizeof storage) {
        printf("footer end: expected offset %zu, got %zu\n",
               sizeof storage,
               footer_end_offset);
        return false;
    }

    footer->size = header->size;
    footer->allocated = header->allocated;

    if (footer->size != 256 || footer->allocated) {
        printf("footer metadata was written incorrectly\n");
        return false;
    }

    return true;
}

static bool test_free_list_operations(void) {
    SegPool pool = {0};

    _Alignas(max_align_t) unsigned char a_storage[256] = {0};
    _Alignas(max_align_t) unsigned char b_storage[256] = {0};
    _Alignas(max_align_t) unsigned char c_storage[256] = {0};

    BlockHeader *A = (BlockHeader *)a_storage;
    BlockHeader *B = (BlockHeader *)b_storage;
    BlockHeader *C = (BlockHeader *)c_storage;

    A->size = 256;
    B->size = 256;
    C->size = 256;

    size_t class_index = size_class_index(256);

    free_list_insert(&pool, A);
    free_list_insert(&pool, B);
    free_list_insert(&pool, C);

    /* Expected: C <-> B <-> A */
    if (pool.free_lists[class_index] != C ||
        block_links(C)->prev != NULL ||
        block_links(C)->next != B ||
        block_links(B)->prev != C ||
        block_links(B)->next != A ||
        block_links(A)->prev != B ||
        block_links(A)->next != NULL) {
        printf("insertion failed\n");
        return false;
    }

    /* Remove middle: C <-> A */
    free_list_remove(&pool, B);

    if (block_links(C)->next != A ||
        block_links(A)->prev != C ||
        block_links(B)->prev != NULL ||
        block_links(B)->next != NULL) {
        printf("middle removal failed\n");
        return false;
    }

    /* Remove head: A */
    free_list_remove(&pool, C);

    if (pool.free_lists[class_index] != A ||
        block_links(A)->prev != NULL) {
        printf("head removal failed\n");
        return false;
    }

    /* Remove only remaining node: empty */
    free_list_remove(&pool, A);

    if (pool.free_lists[class_index] != NULL) {
        printf("final removal failed\n");
        return false;
    }

    return true;
}

static size_t minimum_block_size(void){
    size_t size = (sizeof(BlockHeader)+sizeof(FreeLinks)+sizeof(BlockFooter));
    size_t aligned_size = 0;
    if (!align_up(size,ALIGNMENT, &aligned_size)){
        return 0;
    }

    return aligned_size;

}

static bool seg_pool_init(SegPool *pool, size_t arena_size){
    if (pool == NULL || arena_size < minimum_block_size() || arena_size % ALIGNMENT != 0){
        return false;
    }

    memset(pool,0,sizeof *pool);

    pool -> arena = malloc(arena_size);
    if (pool -> arena == NULL){
        return false;
    }

    pool -> arena_size = arena_size;

    BlockHeader *initial = (BlockHeader *)pool->arena;
    initial -> size = arena_size;
    initial -> allocated = false;
    BlockFooter *footer = block_footer(initial);
    footer -> size = arena_size;
    footer->allocated = false;

    free_list_insert(pool, initial);

    return true;
}

static void seg_pool_destroy(SegPool *pool){
    if (pool == NULL){
        return;
    }

    free(pool->arena);
    memset(pool,0,sizeof *pool);
}

static bool test_pool_init(void) {
    SegPool pool;

    if (!seg_pool_init(&pool, 4096)) {
        printf("pool initialization failed\n");
        return false;
    }

    BlockHeader *initial = (BlockHeader *)pool.arena;
    BlockFooter *footer = block_footer(initial);
    size_t expected_class = size_class_index(4096);

    bool passed =
        initial->size == 4096 &&
        initial->allocated == false &&
        footer->size == 4096 &&
        footer->allocated == false &&
        pool.free_lists[expected_class] == initial &&
        block_links(initial)->prev == NULL &&
        block_links(initial)->next == NULL;

    for (size_t i = 0; i < SIZE_CLASS_COUNT; i++) {
        if (i != expected_class && pool.free_lists[i] != NULL) {
            passed = false;
        }
    }

    if (!passed) {
        printf("initial pool invariant failed\n");
    }

    seg_pool_destroy(&pool);
    return passed;
}

static bool request_to_block_size(size_t request, size_t *result){
    if (request == 0 || result == NULL){
        return false;
    }
    if(request > SIZE_MAX-sizeof(BlockHeader)-sizeof(BlockFooter)){
        return false;
    }
    size_t raw_size = sizeof(BlockHeader)+request+sizeof(BlockFooter);

    if(!align_up(raw_size, ALIGNMENT, result)){
        return false;
    }

    if (*result < min_block_size){
        *result = min_block_size;
    }

    return true;

}

static BlockHeader *find_fit(SegPool *pool, size_t required_size){
    size_t start_class = size_class_index(required_size);
    for (size_t i = start_class; i < SIZE_CLASS_COUNT; i++){
        BlockHeader *current = pool->free_lists[i];

        while (current != NULL){
            if (current->size >= required_size){
                return current;
            }

            current = block_links(current) -> next;
        }
    }

    return NULL;
}

static void write_block(BlockHeader *block, size_t size, bool allocated){
    block -> size = size;
    block-> allocated = allocated;

    BlockFooter *footer = block_footer(block);
    footer -> size = size;
    footer -> allocated = allocated;

}

static void *seg_pool_alloc(SegPool *pool, size_t request) {
    if (pool == NULL) {
        return NULL;
    }

    size_t required_size;

    if (!request_to_block_size(request, &required_size)) {
        return NULL;
    }

    BlockHeader *block = find_fit(pool, required_size);

    if (block == NULL) {
        return NULL;
    }

    free_list_remove(pool, block);

    size_t original_size = block->size;
    size_t remainder_size = original_size - required_size;

    if (remainder_size >= min_block_size) {

        write_block(block, required_size, true);

        BlockHeader *remainder =
            (BlockHeader *)((unsigned char *)block + required_size);

        write_block(remainder, remainder_size, false);
        free_list_insert(pool, remainder);
    } else {

        write_block(block, original_size, true);
    }

    return (unsigned char *)block + sizeof(BlockHeader);
}

static bool test_allocation_split(void) {
    SegPool pool;

    if (!seg_pool_init(&pool, 4096)) {
        return false;
    }

    size_t expected_size;

    if (!request_to_block_size(100, &expected_size)) {
        seg_pool_destroy(&pool);
        return false;
    }

    void *payload = seg_pool_alloc(&pool, 100);

    if (payload == NULL) {
        printf("allocation returned NULL\n");
        seg_pool_destroy(&pool);
        return false;
    }

    BlockHeader *allocated =
        (BlockHeader *)((unsigned char *)payload -
                        sizeof(BlockHeader));

    BlockHeader *remainder =
        (BlockHeader *)((unsigned char *)allocated +
                        expected_size);

    size_t expected_remainder = 4096 - expected_size;
    size_t remainder_class =
        size_class_index(expected_remainder);

    bool passed =
        allocated == (BlockHeader *)pool.arena &&
        allocated->allocated &&
        allocated->size == expected_size &&
        block_footer(allocated)->allocated &&
        block_footer(allocated)->size == expected_size &&

        !remainder->allocated &&
        remainder->size == expected_remainder &&
        !block_footer(remainder)->allocated &&
        block_footer(remainder)->size == expected_remainder &&

        pool.free_lists[remainder_class] == remainder &&
        block_links(remainder)->prev == NULL &&
        block_links(remainder)->next == NULL;

    if (!passed) {
        printf("allocation split invariant failed\n");
    }

    seg_pool_destroy(&pool);
    return passed;
}

static bool seg_pool_free(SegPool *pool, void *payload) {
    if (pool == NULL || pool->arena == NULL || payload == NULL) {
        return false;
    }

    BlockHeader *block =
        (BlockHeader *)((unsigned char *)payload -
                        sizeof(BlockHeader));

    /* Basic double-free rejection. */
    if (!block->allocated) {
        return false;
    }

    unsigned char *block_start = (unsigned char *)block;
    unsigned char *arena_end = pool->arena + pool->arena_size;

    BlockHeader *left = NULL;
    BlockHeader *right = NULL;

    /* Find the left neighbor through its boundary-tag footer. */
    if (block_start > pool->arena) {
        BlockFooter *left_footer =
            (BlockFooter *)(block_start - sizeof(BlockFooter));

        if (!left_footer->allocated) {
            left =
                (BlockHeader *)(block_start - left_footer->size);
        }
    }

    /* Find the right neighbor using the current block's size. */
    unsigned char *right_start = block_start + block->size;

    if (right_start < arena_end) {
        BlockHeader *candidate = (BlockHeader *)right_start;

        if (!candidate->allocated) {
            right = candidate;
        }
    }

    BlockHeader *merged = block;
    size_t merged_size = block->size;

    /*
     * left and right were already free and therefore are currently
     * linked into free lists. Remove them before changing any sizes.
     */
    if (left != NULL) {
        free_list_remove(pool, left);
        merged = left;
        merged_size += left->size;
    }

    if (right != NULL) {
        free_list_remove(pool, right);
        merged_size += right->size;
    }

    write_block(merged, merged_size, false);
    free_list_insert(pool, merged);

    return true;
}
static bool test_free_right_coalesce(void) {
    SegPool pool;

    if (!seg_pool_init(&pool, 4096)) {
        return false;
    }

    void *payload = seg_pool_alloc(&pool, 100);

    if (payload == NULL) {
        seg_pool_destroy(&pool);
        return false;
    }

    if (!seg_pool_free(&pool, payload)) {
        printf("valid free was rejected\n");
        seg_pool_destroy(&pool);
        return false;
    }

    BlockHeader *merged = (BlockHeader *)pool.arena;
    size_t expected_class = size_class_index(4096);

    bool passed =
        !merged->allocated &&
        merged->size == 4096 &&
        !block_footer(merged)->allocated &&
        block_footer(merged)->size == 4096 &&
        pool.free_lists[expected_class] == merged &&
        block_links(merged)->prev == NULL &&
        block_links(merged)->next == NULL;

    for (size_t i = 0; i < SIZE_CLASS_COUNT; i++) {
        if (i != expected_class && pool.free_lists[i] != NULL) {
            passed = false;
        }
    }

    /* The same payload must not be freed twice. */
    if (seg_pool_free(&pool, payload)) {
        printf("double-free was accepted\n");
        passed = false;
    }

    if (!passed) {
        printf("right-coalescing invariant failed\n");
    }

    seg_pool_destroy(&pool);
    return passed;
}
int main(void) {
    bool align_passed = test_align_up();
    bool layout_passed = test_block_layout();
    bool class_passed = test_size_class_index();
    bool list_passed = test_free_list_operations();
    bool init_passed = test_pool_init();
    bool alloc_passed = test_allocation_split();
    bool test_passed = test_free_right_coalesce() ;

    printf("[%s] free-list operations\n",
       list_passed ? "PASS" : "FAIL");

    printf("[%s] size classes\n",
       class_passed ? "PASS" : "FAIL");

    printf("[%s] align_up\n",
           align_passed ? "PASS" : "FAIL");

    printf("[%s] block layout\n",
           layout_passed ? "PASS" : "FAIL");
    printf("[%s] init passed\n",
        init_passed ? "PASS" : "FAIL");
    printf("[%s] alloc passed\n",
        alloc_passed ? "PASS" : "FAIL");
    printf("[%s] test passed\n",
        test_passed ? "PASS" : "FAIL");

    return align_passed && layout_passed
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}

