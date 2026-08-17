#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Reference bounded MPSC ring buffer plus functional tests.
 *
 * Model:
 *   - Multiple producers reserve logical positions through tail CAS.
 *   - One consumer drains positions in increasing head order.
 *   - Logical position k uses physical slot k % capacity.
 *   - sequence == k + 1 means logical position k is published.
 *
 * Assumptions:
 *   - head/tail do not overflow during the test.
 *   - queue destruction does not race with producers.
 *   - full-queue policy is nonblocking failure.
 *   - consumer behavior is Option A: wait at a claimed-but-unpublished head.
 */

typedef struct {
    _Atomic size_t sequence;
    void *payload;
} mpsc_slot_t;

typedef struct {
    size_t position;
    mpsc_slot_t *slot;
} mpsc_reservation_t;

typedef enum {
    MPSC_POP_EMPTY,
    MPSC_POP_NOT_READY,
    MPSC_POP_ITEM
} mpsc_pop_result_t;

typedef struct {
    size_t capacity;
    mpsc_slot_t *slots;
    _Atomic size_t head;
    _Atomic size_t tail;
    pthread_mutex_t sleep_lock;
    pthread_cond_t not_empty;
    _Atomic bool closed;
    _Atomic bool test_consumer_waiting;
} mpsc_queue_t;

static void die_pthread(int rc, const char *what)
{
    if (rc != 0) {
        fprintf(stderr, "%s failed with error %d\n", what, rc);
        abort();
    }
}

static void mpsc_init(mpsc_queue_t *q, size_t capacity)
{
    assert(q != NULL);
    assert(capacity > 0);

    q->capacity = capacity;
    q->slots = calloc(capacity, sizeof(*q->slots));
    if (q->slots == NULL) {
        perror("calloc");
        abort();
    }

    for (size_t i = 0; i < capacity; ++i) {
        atomic_init(&q->slots[i].sequence, 0);
        q->slots[i].payload = NULL;
    }

    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    atomic_init(&q->closed, false);
    atomic_init(&q->test_consumer_waiting, false);

    die_pthread(pthread_mutex_init(&q->sleep_lock, NULL),
                "pthread_mutex_init");
    die_pthread(pthread_cond_init(&q->not_empty, NULL),
                "pthread_cond_init");
}

static void mpsc_close(mpsc_queue_t *q)
{
    atomic_store_explicit(&q->closed, true, memory_order_release);

    die_pthread(pthread_mutex_lock(&q->sleep_lock),
                "pthread_mutex_lock");
    die_pthread(pthread_cond_broadcast(&q->not_empty),
                "pthread_cond_broadcast");
    die_pthread(pthread_mutex_unlock(&q->sleep_lock),
                "pthread_mutex_unlock");
}

static void mpsc_destroy(mpsc_queue_t *q)
{
    die_pthread(pthread_mutex_destroy(&q->sleep_lock),
                "pthread_mutex_destroy");
    die_pthread(pthread_cond_destroy(&q->not_empty),
                "pthread_cond_destroy");
    free(q->slots);
    q->slots = NULL;
    q->capacity = 0;
}

/* Reserve one logical position. The tail CAS is intentionally relaxed. */
static bool mpsc_try_reserve(mpsc_queue_t *q, mpsc_reservation_t *out)
{
    assert(q != NULL);
    assert(out != NULL);

    for (;;) {
        size_t t = atomic_load_explicit(&q->tail,
                                        memory_order_relaxed);
        size_t h = atomic_load_explicit(&q->head,
                                        memory_order_acquire);

        if (t - h >= q->capacity) {
            return false;
        }

        size_t expected = t;
        if (atomic_compare_exchange_weak_explicit(
                &q->tail,
                &expected,
                t + 1,
                memory_order_relaxed,
                memory_order_relaxed)) {
            out->position = t;
            out->slot = &q->slots[t % q->capacity];
            return true;
        }
    }
}

/* Publish payload, then release-store the generation marker. */
static void mpsc_publish(mpsc_queue_t *q,
                         const mpsc_reservation_t *r,
                         void *payload)
{
    assert(q != NULL);
    assert(r != NULL);
    assert(r->slot != NULL);

    r->slot->payload = payload;

    atomic_store_explicit(&r->slot->sequence,
                          r->position + 1,
                          memory_order_release);

    /*
     * The same sleep mutex is used by waiter and signaler to close the
     * check-then-sleep lost-wakeup window.
     */
    die_pthread(pthread_mutex_lock(&q->sleep_lock),
                "pthread_mutex_lock");
    die_pthread(pthread_cond_signal(&q->not_empty),
                "pthread_cond_signal");
    die_pthread(pthread_mutex_unlock(&q->sleep_lock),
                "pthread_mutex_unlock");
}

static bool mpsc_try_push(mpsc_queue_t *q, void *payload)
{
    mpsc_reservation_t r;
    if (!mpsc_try_reserve(q, &r)) {
        return false;
    }

    mpsc_publish(q, &r, payload);
    return true;
}

/*
 * EMPTY: tail == head.
 * NOT_READY: tail > head, but the head generation has not been published.
 * ITEM: expected generation is visible; copy payload and advance head.
 */
static mpsc_pop_result_t mpsc_try_pop(mpsc_queue_t *q, void **out)
{
    assert(q != NULL);
    assert(out != NULL);

    size_t h = atomic_load_explicit(&q->head,
                                    memory_order_relaxed);
    mpsc_slot_t *slot = &q->slots[h % q->capacity];

    size_t sequence = atomic_load_explicit(&slot->sequence,
                                           memory_order_acquire);

    if (sequence == h + 1) {
        void *payload = slot->payload;

        /*
         * Release orders this payload read before a future producer's
         * acquire-read of head and overwrite of the physical slot.
         */
        atomic_store_explicit(&q->head,
                              h + 1,
                              memory_order_release);

        *out = payload;
        return MPSC_POP_ITEM;
    }

    size_t t = atomic_load_explicit(&q->tail,
                                    memory_order_relaxed);

    if (t == h) {
        return MPSC_POP_EMPTY;
    }

    return MPSC_POP_NOT_READY;
}

/* Option A: wait at the current head and never skip to a later position. */
static bool mpsc_pop_wait(mpsc_queue_t *q, void **out)
{
    for (;;) {
        mpsc_pop_result_t result = mpsc_try_pop(q, out);
        if (result == MPSC_POP_ITEM) {
            return true;
        }

        die_pthread(pthread_mutex_lock(&q->sleep_lock),
                    "pthread_mutex_lock");

        for (;;) {
            result = mpsc_try_pop(q, out);
            if (result == MPSC_POP_ITEM) {
                atomic_store_explicit(&q->test_consumer_waiting,
                                      false,
                                      memory_order_release);
                die_pthread(pthread_mutex_unlock(&q->sleep_lock),
                            "pthread_mutex_unlock");
                return true;
            }

            size_t h = atomic_load_explicit(&q->head,
                                            memory_order_relaxed);
            size_t t = atomic_load_explicit(&q->tail,
                                            memory_order_relaxed);
            bool closed = atomic_load_explicit(&q->closed,
                                               memory_order_acquire);

            if (closed && t == h) {
                atomic_store_explicit(&q->test_consumer_waiting,
                                      false,
                                      memory_order_release);
                die_pthread(pthread_mutex_unlock(&q->sleep_lock),
                            "pthread_mutex_unlock");
                return false;
            }

            atomic_store_explicit(&q->test_consumer_waiting,
                                  true,
                                  memory_order_release);

            die_pthread(pthread_cond_wait(&q->not_empty,
                                          &q->sleep_lock),
                        "pthread_cond_wait");

            atomic_store_explicit(&q->test_consumer_waiting,
                                  false,
                                  memory_order_release);
        }
    }
}

/* ---------- Counting and wraparound tests ---------- */

typedef struct {
    mpsc_queue_t *q;
    size_t producer_id;
    size_t tasks_per_producer;
} producer_ctx_t;

typedef struct {
    mpsc_queue_t *q;
    size_t total_tasks;
    unsigned char *seen;
    bool duplicate;
    bool out_of_range;
    size_t consumed;
} counting_consumer_ctx_t;

static void *counting_producer_main(void *arg)
{
    producer_ctx_t *ctx = arg;

    for (size_t local = 0; local < ctx->tasks_per_producer; ++local) {
        size_t id = ctx->producer_id * ctx->tasks_per_producer + local;
        void *payload = (void *)(uintptr_t)(id + 1);

        while (!mpsc_try_push(ctx->q, payload)) {
            sched_yield();
        }
    }

    return NULL;
}

static void *counting_consumer_main(void *arg)
{
    counting_consumer_ctx_t *ctx = arg;

    for (size_t n = 0; n < ctx->total_tasks; ++n) {
        void *payload = NULL;
        bool ok = mpsc_pop_wait(ctx->q, &payload);
        assert(ok);

        uintptr_t raw = (uintptr_t)payload;
        if (raw == 0) {
            ctx->out_of_range = true;
            continue;
        }

        size_t id = (size_t)(raw - 1);
        if (id >= ctx->total_tasks) {
            ctx->out_of_range = true;
            continue;
        }

        if (ctx->seen[id] != 0) {
            ctx->duplicate = true;
        }

        ctx->seen[id]++;
        ctx->consumed++;
    }

    return NULL;
}

static void run_counting_test(const char *name,
                              size_t capacity,
                              size_t nproducers,
                              size_t tasks_per_producer)
{
    printf("[ RUN      ] %s\n", name);

    mpsc_queue_t q;
    mpsc_init(&q, capacity);

    size_t total = nproducers * tasks_per_producer;

    unsigned char *seen = calloc(total, sizeof(*seen));
    pthread_t *producers = calloc(nproducers, sizeof(*producers));
    producer_ctx_t *producer_ctxs =
        calloc(nproducers, sizeof(*producer_ctxs));

    if (seen == NULL || producers == NULL || producer_ctxs == NULL) {
        perror("calloc");
        abort();
    }

    counting_consumer_ctx_t consumer_ctx = {
        .q = &q,
        .total_tasks = total,
        .seen = seen,
        .duplicate = false,
        .out_of_range = false,
        .consumed = 0
    };

    pthread_t consumer;
    die_pthread(pthread_create(&consumer,
                               NULL,
                               counting_consumer_main,
                               &consumer_ctx),
                "pthread_create consumer");

    for (size_t p = 0; p < nproducers; ++p) {
        producer_ctxs[p].q = &q;
        producer_ctxs[p].producer_id = p;
        producer_ctxs[p].tasks_per_producer = tasks_per_producer;

        die_pthread(pthread_create(&producers[p],
                                   NULL,
                                   counting_producer_main,
                                   &producer_ctxs[p]),
                    "pthread_create producer");
    }

    for (size_t p = 0; p < nproducers; ++p) {
        die_pthread(pthread_join(producers[p], NULL),
                    "pthread_join producer");
    }

    die_pthread(pthread_join(consumer, NULL),
                "pthread_join consumer");

    assert(!consumer_ctx.duplicate);
    assert(!consumer_ctx.out_of_range);
    assert(consumer_ctx.consumed == total);

    for (size_t id = 0; id < total; ++id) {
        assert(seen[id] == 1);
    }

    size_t h = atomic_load_explicit(&q.head, memory_order_relaxed);
    size_t t = atomic_load_explicit(&q.tail, memory_order_relaxed);
    assert(h == total);
    assert(t == total);

    mpsc_close(&q);
    mpsc_destroy(&q);

    free(producer_ctxs);
    free(producers);
    free(seen);

    printf("[       OK ] %s "
           "(capacity=%zu, producers=%zu, tasks=%zu)\n",
           name,
           capacity,
           nproducers,
           total);
}

/* ---------- Exact full-boundary test ---------- */

static void test_full_queue_boundary(void)
{
    const char *name = "full_queue_boundary";
    printf("[ RUN      ] %s\n", name);

    enum { CAPACITY = 8 };

    mpsc_queue_t q;
    mpsc_init(&q, CAPACITY);

    for (size_t id = 0; id < CAPACITY; ++id) {
        assert(mpsc_try_push(&q,
                             (void *)(uintptr_t)(id + 1)));
    }

    size_t tail_before =
        atomic_load_explicit(&q.tail, memory_order_relaxed);

    mpsc_reservation_t rejected;
    assert(!mpsc_try_reserve(&q, &rejected));

    size_t tail_after =
        atomic_load_explicit(&q.tail, memory_order_relaxed);

    assert(tail_before == CAPACITY);
    assert(tail_after == tail_before);

    void *payload = NULL;
    assert(mpsc_try_pop(&q, &payload) == MPSC_POP_ITEM);
    assert((uintptr_t)payload == 1);

    assert(mpsc_try_push(&q,
                         (void *)(uintptr_t)(CAPACITY + 1)));

    for (size_t expected = 2;
         expected <= CAPACITY + 1;
         ++expected) {
        payload = NULL;
        assert(mpsc_try_pop(&q, &payload) == MPSC_POP_ITEM);
        assert((uintptr_t)payload == expected);
    }

    payload = NULL;
    assert(mpsc_try_pop(&q, &payload) == MPSC_POP_EMPTY);

    mpsc_close(&q);
    mpsc_destroy(&q);

    printf("[       OK ] %s\n", name);
}

/* ---------- Stalled-producer / not-ready test ---------- */

typedef struct {
    mpsc_queue_t *q;
    _Atomic bool started;
    _Atomic size_t consumed;
    uintptr_t output[2];
} stalled_consumer_ctx_t;

static void *stalled_consumer_main(void *arg)
{
    stalled_consumer_ctx_t *ctx = arg;

    atomic_store_explicit(&ctx->started,
                          true,
                          memory_order_release);

    for (size_t i = 0; i < 2; ++i) {
        void *payload = NULL;
        bool ok = mpsc_pop_wait(ctx->q, &payload);
        assert(ok);

        ctx->output[i] = (uintptr_t)payload;
        atomic_fetch_add_explicit(&ctx->consumed,
                                  1,
                                  memory_order_release);
    }

    return NULL;
}

static void test_stalled_producer_fifo(void)
{
    const char *name = "stalled_producer_fifo";
    printf("[ RUN      ] %s\n", name);

    mpsc_queue_t q;
    mpsc_init(&q, 8);

    mpsc_reservation_t r0;
    assert(mpsc_try_reserve(&q, &r0));
    assert(r0.position == 0);

    mpsc_reservation_t r1;
    assert(mpsc_try_reserve(&q, &r1));
    assert(r1.position == 1);
    mpsc_publish(&q, &r1, (void *)(uintptr_t)2);

    void *payload = NULL;
    assert(mpsc_try_pop(&q, &payload) == MPSC_POP_NOT_READY);

    stalled_consumer_ctx_t ctx = {
        .q = &q,
        .output = {0, 0}
    };
    atomic_init(&ctx.started, false);
    atomic_init(&ctx.consumed, 0);

    pthread_t consumer;
    die_pthread(pthread_create(&consumer,
                               NULL,
                               stalled_consumer_main,
                               &ctx),
                "pthread_create stalled consumer");

    while (!atomic_load_explicit(&ctx.started,
                                 memory_order_acquire)) {
        sched_yield();
    }

    while (!atomic_load_explicit(&q.test_consumer_waiting,
                                 memory_order_acquire)) {
        sched_yield();
    }

    /* Position 1 is ready, but the consumer must not skip position 0. */
    assert(atomic_load_explicit(&ctx.consumed,
                                memory_order_acquire) == 0);
    assert(atomic_load_explicit(&q.head,
                                memory_order_relaxed) == 0);

    /* Release the stalled producer. */
    mpsc_publish(&q, &r0, (void *)(uintptr_t)1);

    die_pthread(pthread_join(consumer, NULL),
                "pthread_join stalled consumer");

    assert(atomic_load_explicit(&ctx.consumed,
                                memory_order_acquire) == 2);
    assert(ctx.output[0] == 1);
    assert(ctx.output[1] == 2);

    assert(atomic_load_explicit(&q.head,
                                memory_order_relaxed) == 2);
    assert(atomic_load_explicit(&q.tail,
                                memory_order_relaxed) == 2);

    mpsc_close(&q);
    mpsc_destroy(&q);

    printf("[       OK ] %s\n", name);
}

int main(void)
{
    run_counting_test("counting_under_contention",
                      64,
                      8,
                      10000);

    run_counting_test("tiny_ring_wraparound",
                      8,
                      4,
                      20000);

    test_full_queue_boundary();
    test_stalled_producer_fifo();

    puts("\nAll MPSC functional tests passed.");
    return 0;
}