/*
 * mpsc_stub_weak.c — NEGATIVE CONTROL: marker publication weakened release->relaxed for the MPSC ring buffer.
 * [Scaffolding — Claude, at N's request; per AI_PROVENANCE. The atomics
 *  protocol below is N's committed artifact, orderings copied VERBATIM from
 *  ring_buffer.c: reserve = relaxed tail load / acquire head load / relaxed
 *  CAS; publish = plain payload store, then RELEASE sequence store; pop =
 *  relaxed head load / ACQUIRE sequence load / plain payload read / RELEASE
 *  head store; empty-check tail load relaxed.]
 *
 * Stripped: the condvar sleep path (mutex-guarded standard pattern; not the
 * lock-free protocol under test), printf, malloc, harness.
 *
 * Instance: N=2, producer P1 pushes {1}, producer P2 pushes {2,3} in order,
 * consumer pops 3. Position 2 reuses physical slot 0 => the reuse chain
 * ((c)#4: consumer head release -> producer head acquire) IS exercised.
 *
 * Blocking is modeled with __VERIFIER_assume: executions where the awaited
 * condition doesn't hold at the check are discarded; all schedules where it
 * does hold are explored. This keeps the execution space finite while
 * preserving exactly the interleavings the obligations quantify over.
 *
 * Asserts encode the obligations:
 *   - each payload {1,2,3} consumed exactly once (no-drop / no-dup);
 *   - payload matches its position's claim (visibility: the negative
 *     control's tripwire — garbage/stale behind a valid marker fires here);
 *   - P2's items arrive in P2's order (FIFO per claim order);
 *   - final head == tail == 3.
 */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef GENMC
#include <genmc.h>
#define ASSUME(c) __VERIFIER_assume(c)
#else
#include <sched.h>
#define ASSUME(c) do { } while (0)   /* native smoke: spin instead, below */
#endif

#define CAP 2u

typedef struct {
    _Atomic size_t sequence;
    void *payload;
} slot_t;

static slot_t slots[CAP];
static _Atomic size_t head;
static _Atomic size_t tail;

/* ---- protocol under test: orderings verbatim ---- */

static bool try_reserve(size_t *pos)
{
    size_t t = atomic_load_explicit(&tail, memory_order_relaxed);
    size_t h = atomic_load_explicit(&head, memory_order_acquire);
    if (t - h >= CAP)
        return false;
    size_t expected = t;
    if (atomic_compare_exchange_weak_explicit(&tail, &expected, t + 1,
                                              memory_order_relaxed,
                                              memory_order_relaxed)) {
        *pos = t;
        return true;
    }
    return false;
}

static void publish(size_t pos, void *payload)
{
    slot_t *s = &slots[pos % CAP];
    s->payload = payload;
    atomic_store_explicit(&s->sequence, pos + 1, memory_order_relaxed); /* NEGATIVE CONTROL: was release */
}

static int try_pop(void **out)   /* 1 = item, 0 = not now */
{
    size_t h = atomic_load_explicit(&head, memory_order_relaxed);
    slot_t *s = &slots[h % CAP];
    size_t seq = atomic_load_explicit(&s->sequence, memory_order_acquire);
    if (seq == h + 1) {
        void *payload = s->payload;
        atomic_store_explicit(&head, h + 1, memory_order_release);
        *out = payload;
        return 1;
    }
    return 0;
}

/* ---- bounded/assume wrappers ---- */

static void push_wait(void *payload)
{
    size_t pos;
#ifdef GENMC
    bool ok = try_reserve(&pos);
    ASSUME(ok);
#else
    while (!try_reserve(&pos)) sched_yield();
#endif
    publish(pos, payload);
}

static void *pop_wait_one(void)
{
    void *out = NULL;
#ifdef GENMC
    int r = try_pop(&out);
    ASSUME(r == 1);
#else
    while (!try_pop(&out)) sched_yield();
#endif
    return out;
}

/* ---- threads ---- */

static uintptr_t got[3];

static void *p1_main(void *arg) { (void)arg; push_wait((void *)(uintptr_t)1); return NULL; }
static void *p2_main(void *arg)
{
    (void)arg;
    push_wait((void *)(uintptr_t)2);
    push_wait((void *)(uintptr_t)3);   /* position 2 -> physical slot 0: reuse */
    return NULL;
}
static void *c_main(void *arg)
{
    (void)arg;
    for (int i = 0; i < 3; i++)
        got[i] = (uintptr_t)pop_wait_one();
    return NULL;
}

int main(void)
{
    for (size_t i = 0; i < CAP; i++) {
        atomic_init(&slots[i].sequence, 0);
        slots[i].payload = NULL;
    }
    atomic_init(&head, 0);
    atomic_init(&tail, 0);

    pthread_t p1, p2, c;
    pthread_create(&c,  NULL, c_main,  NULL);
    pthread_create(&p1, NULL, p1_main, NULL);
    pthread_create(&p2, NULL, p2_main, NULL);
    pthread_join(p1, NULL);
    pthread_join(p2, NULL);
    pthread_join(c,  NULL);

    /* no-drop / no-dup: multiset {1,2,3} exactly */
    int tally[4] = {0, 0, 0, 0};
    for (int i = 0; i < 3; i++) {
        assert(got[i] >= 1 && got[i] <= 3);   /* garbage payload fires HERE */
        tally[got[i]]++;
    }
    assert(tally[1] == 1 && tally[2] == 1 && tally[3] == 1);

    /* FIFO per claim order within P2: 2 before 3 */
    int i2 = -1, i3 = -1;
    for (int i = 0; i < 3; i++) {
        if (got[i] == 2) i2 = i;
        if (got[i] == 3) i3 = i;
    }
    assert(i2 < i3);

    assert(atomic_load_explicit(&head, memory_order_relaxed) == 3);
    assert(atomic_load_explicit(&tail, memory_order_relaxed) == 3);
    return 0;
}
