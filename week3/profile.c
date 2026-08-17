#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#endif

/*
 * Include the submitted implementations directly.  This preserves their
 * behavior and lets the profiler inspect the segregated pool's static block
 * metadata without forcing an API redesign before the experiment.
 */
#define main segregated_pool_vendor_main
#include "../vendor/segregated_pool.c"
#undef main
#undef ALIGNMENT
#undef min_block_size

#define Arena FixedArena
#define arena_init fixed_arena_init
#define arena_destroy fixed_arena_destroy
#define main fixed_pool_vendor_main
#include "../vendor/fixed_pool.c"
#undef main
#undef arena_destroy
#undef arena_init
#undef Arena

#define Arena BumpArena
#define align_up bump_align_up
#define arena_alloc bump_arena_alloc
#define arena_reset bump_arena_reset
#define main bump_allocator_vendor_main
#include "../vendor/bumpallocator.c"
#undef main
#undef arena_reset
#undef arena_alloc
#undef align_up
#undef Arena

enum {
    DEFAULT_OPERATIONS = 1000,
    DEFAULT_SEEDS = 30,
    DEFAULT_BENCHMARK_REPETITIONS = 1000,
    REQUEST_CLASS_MIN = 1,
    REQUEST_CLASS_MAX = 12
};

static const size_t DEFAULT_SEG_ARENA_SIZE = ((size_t)1 << 14);
static const size_t DEFAULT_KV_ARENA_SIZE = ((size_t)1 << 18);
static const size_t DEFAULT_KV_BLOCK_SIZE = 256;
static volatile uintptr_t benchmark_sink = 0;

typedef enum {
    OP_ALLOC = 1,
    OP_FREE = 2
} OpKind;

typedef struct {
    OpKind kind;
    size_t object_id;
    size_t requested_size;
    unsigned request_class;
} TraceOp;

typedef struct {
    TraceOp *ops;
    size_t operation_count;
    size_t object_count;
} Trace;

typedef struct {
    void *pointer;
    size_t requested_size;
    bool live;
} ObjectState;

typedef struct {
    size_t operations;
    size_t seeds;
    size_t benchmark_repetitions;
    size_t seg_arena_size;
    size_t kv_arena_size;
    size_t kv_block_size;
    const char *output_dir;
    bool run_self_tests;
    bool run_experiment_a;
    bool run_experiment_b;
} Config;

typedef struct {
    size_t total_free_extent;
    size_t largest_free_extent;
    size_t allocated_extent;
    size_t allocated_payload_capacity;
    size_t live_requested;
    size_t internal_waste;
    size_t metadata_bytes;
    size_t allocated_blocks;
    size_t free_blocks;
    bool layout_valid;
} SegMetrics;

typedef enum {
    ALLOCATOR_FIXED,
    ALLOCATOR_MALLOC,
    ALLOCATOR_BUMP
} AllocatorKind;

typedef struct {
    AllocatorKind kind;
    const char *name;
    size_t arena_size;
    size_t block_size;
    FixedPool fixed;
    BumpArena bump;
} AllocatorContext;

typedef struct {
    size_t first_failure_op;
    size_t successful_allocations;
    size_t failed_allocations;
    size_t frees;
    size_t peak_live_requested;
    size_t peak_retained;
    size_t final_live_requested;
    size_t final_retained;
    size_t final_internal_waste;
    size_t final_unreclaimed_dead;
} BehaviorSummary;

static const char *allocator_name(AllocatorKind kind) {
    switch (kind) {
        case ALLOCATOR_FIXED: return "fixed_pool";
        case ALLOCATOR_MALLOC: return "malloc";
        case ALLOCATOR_BUMP: return "bump";
    }
    return "unknown";
}

static const char *op_name(OpKind kind) {
    return kind == OP_ALLOC ? "alloc" : "free";
}

static bool checked_mul_size(size_t a, size_t b, size_t *result) {
    if (result == NULL || (a != 0 && b > SIZE_MAX / a)) {
        return false;
    }
    *result = a * b;
    return true;
}

static bool parse_size(const char *text, size_t *result) {
    if (text == NULL || result == NULL || text[0] == '\0') {
        return false;
    }
    errno = 0;
    char *end = NULL;
    uintmax_t value = strtoumax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > SIZE_MAX) {
        return false;
    }
    *result = (size_t)value;
    return true;
}

static uint64_t splitmix64_next(uint64_t *state) {
    uint64_t z = (*state += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

static uint64_t uniform_bounded(uint64_t *state, uint64_t bound) {
    if (bound == 0) {
        return 0;
    }
    const uint64_t threshold = (uint64_t)(-bound) % bound;
    for (;;) {
        uint64_t value = splitmix64_next(state);
        if (value >= threshold) {
            return value % bound;
        }
    }
}

static unsigned sample_request_class(uint64_t *rng) {
    double weights[REQUEST_CLASS_MAX + 1] = {0};
    double normalizer = 0.0;
    for (unsigned n = REQUEST_CLASS_MIN; n <= REQUEST_CLASS_MAX; n++) {
        weights[n] = 1.0 / (double)(n + 1);
        normalizer += weights[n];
    }

    const uint64_t random_bits = splitmix64_next(rng) >> 11;
    const double unit = (double)random_bits * (1.0 / 9007199254740992.0);
    const double target = unit * normalizer;
    double cumulative = 0.0;
    for (unsigned n = REQUEST_CLASS_MIN; n <= REQUEST_CLASS_MAX; n++) {
        cumulative += weights[n];
        if (target < cumulative) {
            return n;
        }
    }
    return REQUEST_CLASS_MAX;
}

static size_t sample_request_size(uint64_t *rng, unsigned request_class) {
    const size_t upper = (size_t)1 << request_class;
    const size_t lower = ((size_t)1 << (request_class - 1)) + 1;
    const size_t width = upper - lower + 1;
    return lower + (size_t)uniform_bounded(rng, width);
}

static void trace_destroy(Trace *trace) {
    if (trace != NULL) {
        free(trace->ops);
        *trace = (Trace){0};
    }
}

static bool generate_trace(Trace *trace, size_t operation_count,
                           uint64_t seed, bool uniform, size_t uniform_size) {
    if (trace == NULL || operation_count == 0 || (uniform && uniform_size == 0)) {
        return false;
    }
    *trace = (Trace){0};
    trace->ops = calloc(operation_count, sizeof *trace->ops);
    size_t *fifo = calloc(operation_count, sizeof *fifo);
    if (trace->ops == NULL || fifo == NULL) {
        free(trace->ops);
        free(fifo);
        *trace = (Trace){0};
        return false;
    }

    uint64_t rng = seed;
    size_t fifo_head = 0;
    size_t fifo_tail = 0;
    size_t next_object_id = 0;

    for (size_t index = 0; index < operation_count; index++) {
        const size_t one_based = index + 1;
        TraceOp *op = &trace->ops[index];

        if (one_based % 4 == 0 && fifo_head < fifo_tail) {
            op->kind = OP_FREE;
            op->object_id = fifo[fifo_head++];
            continue;
        }

        op->kind = OP_ALLOC;
        op->object_id = next_object_id++;
        if (uniform) {
            op->requested_size = uniform_size;
            op->request_class = 0;
        } else {
            op->request_class = sample_request_class(&rng);
            op->requested_size = sample_request_size(&rng, op->request_class);
        }
        fifo[fifo_tail++] = op->object_id;
    }

    free(fifo);
    trace->operation_count = operation_count;
    trace->object_count = next_object_id;
    return true;
}

static bool validate_trace(const Trace *trace, bool uniform, size_t uniform_size) {
    if (trace == NULL || trace->ops == NULL) {
        return false;
    }
    bool *seen = calloc(trace->object_count, sizeof *seen);
    if (seen == NULL) {
        return false;
    }
    bool valid = true;
    for (size_t i = 0; i < trace->operation_count && valid; i++) {
        const TraceOp *op = &trace->ops[i];
        if ((i + 1) % 4 == 0) {
            valid = op->kind == OP_FREE;
        }
        if (op->object_id >= trace->object_count) {
            valid = false;
            break;
        }
        if (op->kind == OP_ALLOC) {
            valid = !seen[op->object_id] && op->requested_size > 0;
            if (uniform) {
                valid = valid && op->requested_size == uniform_size;
            }
            seen[op->object_id] = true;
        } else {
            valid = seen[op->object_id];
        }
    }
    free(seen);
    return valid;
}

static bool traces_equal(const Trace *left, const Trace *right) {
    if (left == NULL || right == NULL ||
        left->operation_count != right->operation_count ||
        left->object_count != right->object_count) {
        return false;
    }
    for (size_t i = 0; i < left->operation_count; i++) {
        const TraceOp *a = &left->ops[i];
        const TraceOp *b = &right->ops[i];
        if (a->kind != b->kind || a->object_id != b->object_id ||
            a->requested_size != b->requested_size ||
            a->request_class != b->request_class) {
            return false;
        }
    }
    return true;
}

static bool test_trace_generator(void) {
    Trace first = {0};
    Trace same = {0};
    Trace different = {0};
    Trace uniform = {0};
    bool passed =
        generate_trace(&first, 1000, 7, false, 0) &&
        generate_trace(&same, 1000, 7, false, 0) &&
        generate_trace(&different, 1000, 8, false, 0) &&
        generate_trace(&uniform, 1000, 9, true, 256);
    if (passed) {
        passed =
            validate_trace(&first, false, 0) &&
            validate_trace(&same, false, 0) &&
            validate_trace(&different, false, 0) &&
            validate_trace(&uniform, true, 256) &&
            traces_equal(&first, &same) &&
            !traces_equal(&first, &different);
    }
    trace_destroy(&first);
    trace_destroy(&same);
    trace_destroy(&different);
    trace_destroy(&uniform);
    return passed;
}

static FILE *open_output(const char *directory, const char *filename) {
    const size_t needed = strlen(directory) + strlen(filename) + 2;
    char *path = malloc(needed);
    if (path == NULL) {
        return NULL;
    }
    snprintf(path, needed, "%s/%s", directory, filename);
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "could not open %s: %s\n", path, strerror(errno));
    }
    free(path);
    return file;
}

static bool measure_seg_pool(const SegPool *pool, const ObjectState *objects,
                             size_t object_count, SegMetrics *metrics) {
    if (pool == NULL || pool->arena == NULL || metrics == NULL) {
        return false;
    }
    *metrics = (SegMetrics){.layout_valid = true};
    const unsigned char *cursor = pool->arena;
    const unsigned char *end = pool->arena + pool->arena_size;

    while (cursor < end) {
        if ((size_t)(end - cursor) < sizeof(BlockHeader)) {
            metrics->layout_valid = false;
            break;
        }
        const BlockHeader *header = (const BlockHeader *)cursor;
        if (header->size < minimum_block_size() ||
            header->size % _Alignof(max_align_t) != 0 ||
            header->size > (size_t)(end - cursor)) {
            metrics->layout_valid = false;
            break;
        }
        const BlockFooter *footer =
            (const BlockFooter *)(cursor + header->size - sizeof(BlockFooter));
        if (footer->size != header->size || footer->allocated != header->allocated) {
            metrics->layout_valid = false;
            break;
        }

        metrics->metadata_bytes += sizeof(BlockHeader) + sizeof(BlockFooter);
        if (header->allocated) {
            metrics->allocated_blocks++;
            metrics->allocated_extent += header->size;
        } else {
            metrics->free_blocks++;
            metrics->total_free_extent += header->size;
            if (header->size > metrics->largest_free_extent) {
                metrics->largest_free_extent = header->size;
            }
            metrics->metadata_bytes += sizeof(FreeLinks);
        }
        cursor += header->size;
    }
    if (cursor != end) {
        metrics->layout_valid = false;
    }

    for (size_t i = 0; i < object_count; i++) {
        if (!objects[i].live || objects[i].pointer == NULL) {
            continue;
        }
        const BlockHeader *header = (const BlockHeader *)
            ((const unsigned char *)objects[i].pointer - sizeof(BlockHeader));
        if (!header->allocated || header->size < sizeof(BlockHeader) + sizeof(BlockFooter)) {
            metrics->layout_valid = false;
            continue;
        }
        const size_t capacity =
            header->size - sizeof(BlockHeader) - sizeof(BlockFooter);
        metrics->live_requested += objects[i].requested_size;
        metrics->allocated_payload_capacity += capacity;
        if (capacity >= objects[i].requested_size) {
            metrics->internal_waste += capacity - objects[i].requested_size;
        } else {
            metrics->layout_valid = false;
        }
    }
    return metrics->layout_valid;
}

static const char *classify_failure(size_t total_free, size_t largest_free,
                                    size_t required_size) {
    if (total_free < required_size) {
        return "capacity";
    }
    if (largest_free < required_size) {
        return "external_fragmentation";
    }
    return "allocator_policy_or_bug";
}

static bool run_experiment_a(const Config *config) {
    FILE *operations = open_output(config->output_dir, "experiment_a_operations.csv");
    FILE *summaries = open_output(config->output_dir, "experiment_a_seed_summary.csv");
    if (operations == NULL || summaries == NULL) {
        if (operations != NULL) fclose(operations);
        if (summaries != NULL) fclose(summaries);
        return false;
    }

    fprintf(operations,
        "seed,operation_index,op_type,object_id,requested_size,request_class,"
        "required_block_size,allocator_class,success,failure_type,total_free_extent,"
        "largest_free_extent,live_requested,allocated_payload_capacity,internal_waste,"
        "internal_fragmentation_ratio,external_fragmentation,utilization,metadata_bytes,"
        "allocated_blocks,free_blocks,layout_valid\n");
    fprintf(summaries,
        "seed,first_fragmentation_op,first_request_class,first_allocator_class,"
        "first_requested_size,first_required_block_size,first_total_free,"
        "first_largest_free,allocation_successes,capacity_failures,"
        "fragmentation_failures,bug_or_policy_failures,layout_valid\n");

    bool all_valid = true;
    for (size_t seed_index = 0; seed_index < config->seeds; seed_index++) {
        const uint64_t seed = (uint64_t)(seed_index + 1);
        Trace trace = {0};
        if (!generate_trace(&trace, config->operations, seed, false, 0) ||
            !validate_trace(&trace, false, 0)) {
            fprintf(stderr, "failed to generate varied trace for seed %" PRIu64 "\n", seed);
            all_valid = false;
            trace_destroy(&trace);
            break;
        }
        ObjectState *objects = calloc(trace.object_count, sizeof *objects);
        SegPool pool = {0};
        if (objects == NULL || !seg_pool_init(&pool, config->seg_arena_size)) {
            fprintf(stderr, "failed to initialize Experiment A for seed %" PRIu64 "\n", seed);
            free(objects);
            trace_destroy(&trace);
            all_valid = false;
            break;
        }

        size_t first_frag_op = 0;
        unsigned first_request_class = 0;
        size_t first_allocator_class = 0;
        size_t first_requested = 0;
        size_t first_required = 0;
        size_t first_total_free = 0;
        size_t first_largest_free = 0;
        size_t successes = 0;
        size_t capacity_failures = 0;
        size_t fragmentation_failures = 0;
        size_t bug_failures = 0;
        bool layout_valid = true;

        for (size_t i = 0; i < trace.operation_count; i++) {
            const TraceOp *op = &trace.ops[i];
            bool success = true;
            const char *failure_type = "none";
            size_t required_size = 0;
            size_t allocator_class = 0;

            if (op->kind == OP_ALLOC) {
                if (!request_to_block_size(op->requested_size, &required_size)) {
                    success = false;
                    failure_type = "invalid_request";
                } else {
                    allocator_class = size_class_index(required_size);
                    SegMetrics before = {0};
                    layout_valid = measure_seg_pool(&pool, objects, trace.object_count, &before)
                                   && layout_valid;
                    void *pointer = seg_pool_alloc(&pool, op->requested_size);
                    success = pointer != NULL;
                    if (success) {
                        objects[op->object_id] = (ObjectState){
                            .pointer = pointer,
                            .requested_size = op->requested_size,
                            .live = true
                        };
                        successes++;
                    } else {
                        failure_type = classify_failure(before.total_free_extent,
                                                        before.largest_free_extent,
                                                        required_size);
                        if (strcmp(failure_type, "capacity") == 0) {
                            capacity_failures++;
                        } else if (strcmp(failure_type, "external_fragmentation") == 0) {
                            fragmentation_failures++;
                            if (first_frag_op == 0) {
                                first_frag_op = i + 1;
                                first_request_class = op->request_class;
                                first_allocator_class = allocator_class;
                                first_requested = op->requested_size;
                                first_required = required_size;
                                first_total_free = before.total_free_extent;
                                first_largest_free = before.largest_free_extent;
                            }
                        } else {
                            bug_failures++;
                        }
                    }
                }
            } else {
                ObjectState *object = &objects[op->object_id];
                if (object->live && object->pointer != NULL) {
                    success = seg_pool_free(&pool, object->pointer);
                    if (success) {
                        *object = (ObjectState){0};
                    } else {
                        failure_type = "free_error";
                    }
                } else {
                    /* A logical free for an allocation that previously failed. */
                    success = true;
                }
            }

            SegMetrics metrics = {0};
            layout_valid = measure_seg_pool(&pool, objects, trace.object_count, &metrics)
                           && layout_valid;
            const double internal_ratio = metrics.allocated_payload_capacity == 0
                ? 0.0
                : (double)metrics.internal_waste /
                  (double)metrics.allocated_payload_capacity;
            const double external = metrics.total_free_extent == 0
                ? 0.0
                : 1.0 - (double)metrics.largest_free_extent /
                        (double)metrics.total_free_extent;
            const double utilization = 1.0 -
                (double)metrics.total_free_extent / (double)pool.arena_size;

            fprintf(operations,
                "%" PRIu64 ",%zu,%s,%zu,%zu,%u,%zu,%zu,%d,%s,%zu,%zu,%zu,%zu,"
                "%zu,%.9f,%.9f,%.9f,%zu,%zu,%zu,%d\n",
                seed, i + 1, op_name(op->kind), op->object_id,
                op->requested_size, op->request_class, required_size, allocator_class,
                success ? 1 : 0, failure_type, metrics.total_free_extent,
                metrics.largest_free_extent, metrics.live_requested,
                metrics.allocated_payload_capacity, metrics.internal_waste,
                internal_ratio, external, utilization, metrics.metadata_bytes,
                metrics.allocated_blocks, metrics.free_blocks,
                metrics.layout_valid ? 1 : 0);
        }

        fprintf(summaries,
            "%" PRIu64 ",%zu,%u,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%d\n",
            seed, first_frag_op, first_request_class, first_allocator_class,
            first_requested, first_required, first_total_free, first_largest_free,
            successes, capacity_failures, fragmentation_failures, bug_failures,
            layout_valid ? 1 : 0);

        all_valid = all_valid && layout_valid && bug_failures == 0;
        seg_pool_destroy(&pool);
        free(objects);
        trace_destroy(&trace);
    }

    fclose(operations);
    fclose(summaries);
    return all_valid;
}

static size_t malloc_usable_capacity(void *pointer, size_t requested) {
    if (pointer == NULL) {
        return 0;
    }
#if defined(__APPLE__)
    (void)requested;
    return malloc_size(pointer);
#elif defined(__linux__)
    (void)requested;
    return malloc_usable_size(pointer);
#else
    return requested;
#endif
}

static bool allocator_init(AllocatorContext *context, AllocatorKind kind,
                           size_t arena_size, size_t block_size) {
    if (context == NULL) {
        return false;
    }
    *context = (AllocatorContext){
        .kind = kind,
        .name = allocator_name(kind),
        .arena_size = arena_size,
        .block_size = block_size
    };
    if (kind == ALLOCATOR_FIXED) {
        return fixed_pool_init(&context->fixed, arena_size, block_size);
    }
    if (kind == ALLOCATOR_BUMP) {
        context->bump.base = malloc(arena_size);
        if (context->bump.base == NULL) {
            return false;
        }
        context->bump.size = arena_size;
        context->bump.offset = 0;
    }
    return true;
}

static void allocator_destroy(AllocatorContext *context,
                              ObjectState *objects, size_t object_count) {
    if (context == NULL) {
        return;
    }
    if (context->kind == ALLOCATOR_MALLOC && objects != NULL) {
        for (size_t i = 0; i < object_count; i++) {
            if (objects[i].pointer != NULL) {
                free(objects[i].pointer);
                objects[i].pointer = NULL;
            }
        }
    } else if (context->kind == ALLOCATOR_FIXED) {
        fixed_pool_destroy(&context->fixed);
    } else if (context->kind == ALLOCATOR_BUMP) {
        free(context->bump.base);
        context->bump = (BumpArena){0};
    }
}

static void *allocator_alloc(AllocatorContext *context, size_t requested) {
    switch (context->kind) {
        case ALLOCATOR_FIXED:
            return requested <= context->fixed.block_size
                ? fixed_pool_alloc(&context->fixed) : NULL;
        case ALLOCATOR_MALLOC:
            return malloc(requested);
        case ALLOCATOR_BUMP:
            return bump_arena_alloc(&context->bump, requested,
                                    _Alignof(max_align_t));
    }
    return NULL;
}

static bool allocator_free(AllocatorContext *context, void *pointer) {
    switch (context->kind) {
        case ALLOCATOR_FIXED:
            return fixed_pool_free(&context->fixed, pointer);
        case ALLOCATOR_MALLOC:
            free(pointer);
            return true;
        case ALLOCATOR_BUMP:
            /* Individual frees are intentionally unsupported. */
            (void)pointer;
            return true;
    }
    return false;
}

static void behavior_metrics(const AllocatorContext *context,
                             const ObjectState *objects, size_t object_count,
                             size_t *live_count, size_t *live_requested,
                             size_t *retained, size_t *internal_waste,
                             size_t *unreclaimed_dead) {
    *live_count = 0;
    *live_requested = 0;
    *retained = 0;
    *internal_waste = 0;
    *unreclaimed_dead = 0;

    for (size_t i = 0; i < object_count; i++) {
        if (!objects[i].live || objects[i].pointer == NULL) {
            continue;
        }
        (*live_count)++;
        *live_requested += objects[i].requested_size;
        if (context->kind == ALLOCATOR_MALLOC) {
            const size_t usable = malloc_usable_capacity(objects[i].pointer,
                                                         objects[i].requested_size);
            *retained += usable;
            if (usable >= objects[i].requested_size) {
                *internal_waste += usable - objects[i].requested_size;
            }
        }
    }

    if (context->kind == ALLOCATOR_FIXED) {
        const size_t live_blocks = context->fixed.block_count - context->fixed.free_count;
        *retained = live_blocks * context->fixed.stride;
        if (*retained >= *live_requested) {
            *internal_waste = *retained - *live_requested;
        }
    } else if (context->kind == ALLOCATOR_BUMP) {
        *retained = context->bump.offset;
        if (*retained >= *live_requested) {
            *unreclaimed_dead = *retained - *live_requested;
        }
    }
}

static bool replay_behavior(const Trace *trace, AllocatorKind kind,
                            const Config *config, FILE *operations,
                            BehaviorSummary *summary) {
    ObjectState *objects = calloc(trace->object_count, sizeof *objects);
    AllocatorContext context = {0};
    if (objects == NULL || !allocator_init(&context, kind,
                                           config->kv_arena_size,
                                           config->kv_block_size)) {
        free(objects);
        return false;
    }
    *summary = (BehaviorSummary){0};

    for (size_t i = 0; i < trace->operation_count; i++) {
        const TraceOp *op = &trace->ops[i];
        bool success = true;
        if (op->kind == OP_ALLOC) {
            void *pointer = allocator_alloc(&context, op->requested_size);
            success = pointer != NULL;
            if (success) {
                objects[op->object_id] = (ObjectState){
                    .pointer = pointer,
                    .requested_size = op->requested_size,
                    .live = true
                };
                summary->successful_allocations++;
            } else {
                summary->failed_allocations++;
                if (summary->first_failure_op == 0) {
                    summary->first_failure_op = i + 1;
                }
            }
        } else {
            ObjectState *object = &objects[op->object_id];
            if (object->live && object->pointer != NULL) {
                success = allocator_free(&context, object->pointer);
                if (success) {
                    summary->frees++;
                    object->live = false;
                    if (kind != ALLOCATOR_BUMP) {
                        object->pointer = NULL;
                    }
                }
            }
        }

        size_t live_count = 0;
        size_t live_requested = 0;
        size_t retained = 0;
        size_t internal_waste = 0;
        size_t unreclaimed_dead = 0;
        behavior_metrics(&context, objects, trace->object_count,
                         &live_count, &live_requested, &retained,
                         &internal_waste, &unreclaimed_dead);
        if (live_requested > summary->peak_live_requested) {
            summary->peak_live_requested = live_requested;
        }
        if (retained > summary->peak_retained) {
            summary->peak_retained = retained;
        }
        summary->final_live_requested = live_requested;
        summary->final_retained = retained;
        summary->final_internal_waste = internal_waste;
        summary->final_unreclaimed_dead = unreclaimed_dead;

        fprintf(operations,
            "%s,%zu,%s,%zu,%zu,%d,%zu,%zu,%zu,%zu,%zu,%zu\n",
            context.name, i + 1, op_name(op->kind), op->object_id,
            op->requested_size, success ? 1 : 0, live_count,
            live_requested, retained, internal_waste, unreclaimed_dead,
            summary->failed_allocations);
    }

    allocator_destroy(&context, objects, trace->object_count);
    free(objects);
    return true;
}

static uint64_t elapsed_ns(struct timespec start, struct timespec end) {
    return (uint64_t)(end.tv_sec - start.tv_sec) * UINT64_C(1000000000)
         + (uint64_t)(end.tv_nsec - start.tv_nsec);
}

static bool benchmark_prefix(const Trace *trace, size_t prefix_operations,
                             AllocatorKind kind, const Config *config,
                             size_t repetition, FILE *output) {
    ObjectState *objects = calloc(trace->object_count, sizeof *objects);
    AllocatorContext context = {0};
    if (objects == NULL || !allocator_init(&context, kind,
                                           config->kv_arena_size,
                                           config->kv_block_size)) {
        free(objects);
        return false;
    }

    uintptr_t checksum = 0;
    bool valid = true;
    struct timespec start = {0};
    struct timespec end = {0};
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (size_t i = 0; i < prefix_operations; i++) {
        const TraceOp *op = &trace->ops[i];
        if (op->kind == OP_ALLOC) {
            void *pointer = allocator_alloc(&context, op->requested_size);
            if (pointer == NULL) {
                valid = false;
                break;
            }
            objects[op->object_id] = (ObjectState){
                .pointer = pointer,
                .requested_size = op->requested_size,
                .live = true
            };
            checksum ^= (uintptr_t)pointer;
        } else {
            ObjectState *object = &objects[op->object_id];
            if (object->live && object->pointer != NULL) {
                const uintptr_t pointer_bits = (uintptr_t)object->pointer;
                valid = allocator_free(&context, object->pointer) && valid;
                checksum ^= pointer_bits >> 4;
                object->live = false;
                if (kind != ALLOCATOR_BUMP) {
                    object->pointer = NULL;
                }
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    benchmark_sink ^= checksum;

    const uint64_t nanoseconds = elapsed_ns(start, end);
    const double throughput = nanoseconds == 0
        ? 0.0
        : (double)prefix_operations * 1e9 / (double)nanoseconds;
    fprintf(output, "%zu,%s,%zu,%" PRIu64 ",%.3f,0x%" PRIxPTR ",%d\n",
            repetition, allocator_name(kind), prefix_operations,
            nanoseconds, throughput, checksum, valid ? 1 : 0);

    allocator_destroy(&context, objects, trace->object_count);
    free(objects);
    return valid;
}

static bool run_experiment_b(const Config *config) {
    Trace trace = {0};
    if (!generate_trace(&trace, config->operations, UINT64_C(0x4b565f5452414345),
                        true, config->kv_block_size) ||
        !validate_trace(&trace, true, config->kv_block_size)) {
        fprintf(stderr, "failed to generate uniform KV trace\n");
        trace_destroy(&trace);
        return false;
    }

    FILE *operations = open_output(config->output_dir, "experiment_b_operations.csv");
    FILE *summaries = open_output(config->output_dir, "experiment_b_summary.csv");
    FILE *benchmark = open_output(config->output_dir, "experiment_b_benchmark.csv");
    if (operations == NULL || summaries == NULL || benchmark == NULL) {
        if (operations != NULL) fclose(operations);
        if (summaries != NULL) fclose(summaries);
        if (benchmark != NULL) fclose(benchmark);
        trace_destroy(&trace);
        return false;
    }

    fprintf(operations,
        "allocator,operation_index,op_type,object_id,requested_size,success,"
        "live_objects,live_requested_bytes,retained_bytes,internal_waste_bytes,"
        "unreclaimed_dead_bytes,cumulative_allocation_failures\n");
    fprintf(summaries,
        "allocator,first_failure_op,successful_allocations,failed_allocations,frees,"
        "peak_live_requested,peak_retained,final_live_requested,final_retained,"
        "final_internal_waste,final_unreclaimed_dead\n");
    fprintf(benchmark,
        "repetition,allocator,operations,elapsed_ns,operations_per_second,checksum,valid\n");

    const AllocatorKind kinds[] = {
        ALLOCATOR_FIXED, ALLOCATOR_MALLOC, ALLOCATOR_BUMP
    };
    BehaviorSummary behavior[3] = {0};
    bool valid = true;
    size_t common_prefix = trace.operation_count;
    for (size_t i = 0; i < 3; i++) {
        valid = replay_behavior(&trace, kinds[i], config, operations, &behavior[i])
                && valid;
        if (behavior[i].first_failure_op != 0 &&
            behavior[i].first_failure_op - 1 < common_prefix) {
            common_prefix = behavior[i].first_failure_op - 1;
        }
        fprintf(summaries,
            "%s,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu\n",
            allocator_name(kinds[i]), behavior[i].first_failure_op,
            behavior[i].successful_allocations, behavior[i].failed_allocations,
            behavior[i].frees, behavior[i].peak_live_requested,
            behavior[i].peak_retained, behavior[i].final_live_requested,
            behavior[i].final_retained, behavior[i].final_internal_waste,
            behavior[i].final_unreclaimed_dead);
    }
    if (common_prefix == 0) {
        fprintf(stderr, "no common successful prefix for throughput benchmark\n");
        valid = false;
    } else {
        for (size_t rep = 0; rep < config->benchmark_repetitions; rep++) {
            /* Rotate order to reduce systematic thermal/order bias. */
            for (size_t offset = 0; offset < 3; offset++) {
                const size_t index = (rep + offset) % 3;
                valid = benchmark_prefix(&trace, common_prefix, kinds[index],
                                         config, rep, benchmark) && valid;
            }
        }
    }

    fclose(operations);
    fclose(summaries);
    fclose(benchmark);
    trace_destroy(&trace);
    return valid;
}

static bool run_self_tests(void) {
    const struct {
        const char *name;
        bool (*run)(void);
    } tests[] = {
        {"seg: align_up", test_align_up},
        {"seg: block layout", test_block_layout},
        {"seg: size classes", test_size_class_index},
        {"seg: free-list operations", test_free_list_operations},
        {"seg: init", test_pool_init},
        {"seg: allocation split", test_allocation_split},
        {"seg: free/coalesce", test_free_right_coalesce},
        {"fixed: initial free list", test_initial_free_list},
        {"fixed: allocation metadata", test_allocation_metadata},
        {"fixed: free/reallocate", test_allocate_free_reallocate},
        {"fixed: exhaustion/recovery", test_exhaustion_uniqueness_recovery},
        {"fixed: NULL block", test_null_block},
        {"fixed: NULL pool", test_null_pool},
        {"fixed: interior pointer", test_interior_pointer},
        {"fixed: one-past pointer", test_one_past_pointer},
        {"fixed: foreign pointer", test_foreign_pointer},
        {"fixed: double free", test_double_free}
    };

    bool passed = true;
    const size_t count = sizeof tests / sizeof tests[0];
    for (size_t i = 0; i < count; i++) {
        const bool result = tests[i].run();
        printf("[%s] %s\n", result ? "PASS" : "FAIL", tests[i].name);
        passed = result && passed;
    }

    BumpArena bump = {
        .base = malloc(128),
        .size = 128,
        .offset = 0
    };
    bool bump_passed = bump.base != NULL;
    if (bump_passed) {
        void *a = bump_arena_alloc(&bump, 1, 1);
        void *b = bump_arena_alloc(&bump, sizeof(double), _Alignof(double));
        void *too_large = bump_arena_alloc(&bump, 129, 1);
        bump_passed = a != NULL && b != NULL &&
                      (uintptr_t)b % _Alignof(double) == 0 && too_large == NULL;
    }
    free(bump.base);
    printf("[%s] bump: alignment/exhaustion\n", bump_passed ? "PASS" : "FAIL");
    const bool trace_passed = test_trace_generator();
    printf("[%s] harness: deterministic FIFO traces\n",
           trace_passed ? "PASS" : "FAIL");
    return passed && bump_passed && trace_passed;
}

static bool write_metadata(const Config *config) {
    FILE *file = open_output(config->output_dir, "run_metadata.json");
    if (file == NULL) {
        return false;
    }
    fprintf(file,
        "{\n"
        "  \"experiment_a\": {\n"
        "    \"arena_size_bytes\": %zu,\n"
        "    \"operations_per_trace\": %zu,\n"
        "    \"seeds\": %zu,\n"
        "    \"seed_values\": \"1..%zu\",\n"
        "    \"request_classes\": \"n=1..12\",\n"
        "    \"class_probability\": \"P(n)=1/[(H_13-1)(n+1)]\",\n"
        "    \"request_within_class\": \"UniformInteger(2^(n-1)+1, 2^n)\",\n"
        "    \"lifetime_rule\": \"every fourth operation frees the oldest logical allocation\",\n"
        "    \"registered_prediction\": \"request class 12 is modal first fragmentation failure\"\n"
        "  },\n"
        "  \"experiment_b\": {\n"
        "    \"arena_size_bytes\": %zu,\n"
        "    \"block_size_bytes\": %zu,\n"
        "    \"operations\": %zu,\n"
        "    \"benchmark_repetitions\": %zu,\n"
        "    \"directional_throughput_prediction\": \"bump > fixed_pool > malloc\",\n"
        "    \"note\": \"malloc uses the system heap; fixed and bump use the configured arena\"\n"
        "  }\n"
        "}\n",
        config->seg_arena_size, config->operations, config->seeds, config->seeds,
        config->kv_arena_size, config->kv_block_size, config->operations,
        config->benchmark_repetitions);
    fclose(file);
    return true;
}

static void print_usage(const char *program) {
    fprintf(stderr,
        "usage: %s [--all|--self-test|--experiment-a|--experiment-b] [options]\n"
        "  --output-dir PATH       CSV/metadata destination (default: results)\n"
        "  --operations N          operations per trace (default: 1000)\n"
        "  --seeds N               Experiment A seeds (default: 30)\n"
        "  --seg-arena-size N      Experiment A bytes (default: 16384)\n"
        "  --kv-arena-size N       Experiment B local-arena bytes (default: 262144)\n"
        "  --kv-block-size N       uniform request bytes (default: 256)\n"
        "  --benchmark-reps N      timing repetitions (default: 1000)\n",
        program);
}

static bool parse_args(int argc, char **argv, Config *config) {
    *config = (Config){
        .operations = DEFAULT_OPERATIONS,
        .seeds = DEFAULT_SEEDS,
        .benchmark_repetitions = DEFAULT_BENCHMARK_REPETITIONS,
        .seg_arena_size = DEFAULT_SEG_ARENA_SIZE,
        .kv_arena_size = DEFAULT_KV_ARENA_SIZE,
        .kv_block_size = DEFAULT_KV_BLOCK_SIZE,
        .output_dir = "results",
        .run_self_tests = true,
        .run_experiment_a = true,
        .run_experiment_b = true
    };

    bool explicit_mode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--all") == 0) {
            config->run_self_tests = true;
            config->run_experiment_a = true;
            config->run_experiment_b = true;
            explicit_mode = true;
        } else if (strcmp(argv[i], "--self-test") == 0 ||
                   strcmp(argv[i], "--experiment-a") == 0 ||
                   strcmp(argv[i], "--experiment-b") == 0) {
            if (!explicit_mode) {
                config->run_self_tests = false;
                config->run_experiment_a = false;
                config->run_experiment_b = false;
                explicit_mode = true;
            }
            if (strcmp(argv[i], "--self-test") == 0) config->run_self_tests = true;
            if (strcmp(argv[i], "--experiment-a") == 0) config->run_experiment_a = true;
            if (strcmp(argv[i], "--experiment-b") == 0) config->run_experiment_b = true;
        } else if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            config->output_dir = argv[++i];
        } else if (i + 1 < argc &&
                   (strcmp(argv[i], "--operations") == 0 ||
                    strcmp(argv[i], "--seeds") == 0 ||
                    strcmp(argv[i], "--seg-arena-size") == 0 ||
                    strcmp(argv[i], "--kv-arena-size") == 0 ||
                    strcmp(argv[i], "--kv-block-size") == 0 ||
                    strcmp(argv[i], "--benchmark-reps") == 0)) {
            size_t value = 0;
            const char *option = argv[i++];
            if (!parse_size(argv[i], &value) || value == 0) {
                fprintf(stderr, "invalid value for %s: %s\n", option, argv[i]);
                return false;
            }
            if (strcmp(option, "--operations") == 0) config->operations = value;
            if (strcmp(option, "--seeds") == 0) config->seeds = value;
            if (strcmp(option, "--seg-arena-size") == 0) config->seg_arena_size = value;
            if (strcmp(option, "--kv-arena-size") == 0) config->kv_arena_size = value;
            if (strcmp(option, "--kv-block-size") == 0) config->kv_block_size = value;
            if (strcmp(option, "--benchmark-reps") == 0) config->benchmark_repetitions = value;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "unknown or incomplete option: %s\n", argv[i]);
            return false;
        }
    }

    size_t total_trace_entries = 0;
    if (!checked_mul_size(config->operations, config->seeds, &total_trace_entries)) {
        fprintf(stderr, "operations * seeds overflows size_t\n");
        return false;
    }
    (void)total_trace_entries;
    return true;
}

int main(int argc, char **argv) {
    Config config = {0};
    if (!parse_args(argc, argv, &config)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    bool passed = true;
    if (config.run_self_tests) {
        printf("running allocator self-tests\n");
        passed = run_self_tests() && passed;
    }
    if (config.run_experiment_a) {
        printf("running Experiment A: %zu seeds x %zu operations\n",
               config.seeds, config.operations);
        passed = run_experiment_a(&config) && passed;
    }
    if (config.run_experiment_b) {
        printf("running Experiment B: %zu operations, %zu timing repetitions\n",
               config.operations, config.benchmark_repetitions);
        passed = run_experiment_b(&config) && passed;
    }
    if (config.run_experiment_a || config.run_experiment_b) {
        passed = write_metadata(&config) && passed;
    }

    printf("profiling %s\n", passed ? "completed successfully" : "failed");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
