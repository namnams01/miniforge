// sweep.c — Week 2 Tue/Wed: + instrumentation to test the wake-path hypothesis.
//
//   cc -O2 -Wall -Wextra sweep.c -o sweep
//
// [EDIT 2026-07-12, measurement scaffolding only — Claude, at N's request; logged
//  per AI_PROVENANCE. Pool logic untouched. Changes:]
//   (1) MEASURED REGION isolated: pool creation, thread spawn, warmup, and
//       teardown/join are now OUTSIDE the clock. t0..t1 covers exactly:
//       submit K tasks -> all K tasks COMPLETED (not queue-empty, not join).
//       Previous version timed submit..pool_destroy, so join of W threads and
//       drain/broadcast overhead (which grow with W) were inside ns/task.
//       pool_submit stays IN the region by design: producer-side lock traffic
//       is part of the phenomenon under study.
//   (2) Completion boundary: per-worker completed-task counters (owner-written,
//       relaxed atomics, one padded line each — no shared-line traffic added).
//       Main thread polls the sum with a 20us nanosleep between polls (sleeping,
//       not spinning, so the poller does not occupy a core at W ~ P-cores).
//       Boundary error <= ~20us against ms-scale regions.
//   (3) Warmup batch per sweep point (dedicated scratch cells) so the region
//       measures steady-state dispatch, per the Week-1 harness discipline.
//   (4) R=5 repeats per point over the SAME live pool; median reported, with
//       max |dev|/median as a jitter column. Per-worker stats digested from the
//       median repeat. E1 discipline: medians, repeated, directional.
//   (5) ntasks increment moved to AFTER task fn returns (completed, not popped)
//       so it doubles as the completion signal. Correctness check unchanged,
//       performed after join as before.
//
// Original per-worker counters (padded, owner-written):
//   ntasks[w] — tasks this worker COMPLETED
//   nsleeps[w] — times this worker entered cond_wait (found queue empty)
//
// Output columns per sweep point:
//   active   — workers that ran >=1% of tasks (size of the "active cast")
//   top      — largest single worker's share of all tasks
//   sleeps/T — total cond_wait entries per task (~1 => one wake round-trip per task)
//   jit%     — max repeat deviation from the median throughput
//
// Hypothesis (wake-path story) predicts at high W: active small and ~constant in W,
// top large, sleeps/T ~= 1. Contention story predicts: active ~= W, top ~= 1/W,
// sleeps/T << 1.

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LINE 128                    /* M4 cache line */
#define MAXW 64
#define REPS 5                      /* repeats per sweep point (median reported) */

/* per-worker stats, one cache line each so owners never share a line.
   relaxed atomics: owner-only writes stay contention-free, but the main
   thread may now legally poll them for the completion boundary. */
typedef struct {
    _Atomic unsigned long ntasks;   /* completed tasks (incremented AFTER fn) */
    _Atomic unsigned long nsleeps;
    char pad[LINE - 2 * sizeof(_Atomic unsigned long)];
} wstat_t;

static wstat_t wstats[MAXW] __attribute__((aligned(LINE)));

/* ===================== the pool (logic unchanged) ===================== */

typedef void (*task_fn)(void *arg);
typedef struct task {
    task_fn fn;
    void *arg;
    struct task *next;
} task_t;
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    task_t *head, *tail;
    bool shutdown;
    int nworkers;
    pthread_t *threads;
    _Atomic int *busy;
} pool_t;
typedef struct { pool_t *pool; int id; } worker_arg_t;

static void *worker_main(void *v) {
    worker_arg_t *wa = v;
    pool_t *p = wa->pool;
    int id = wa->id;
    free(wa);
    for (;;) {
        pthread_mutex_lock(&p->lock);
        if (p->head == NULL && !p->shutdown)
            atomic_fetch_add_explicit(&wstats[id].nsleeps, 1,
                                      memory_order_relaxed);
        while (p->head == NULL && !p->shutdown)
            pthread_cond_wait(&p->not_empty, &p->lock);
        if (p->head == NULL && p->shutdown) {
            pthread_mutex_unlock(&p->lock);
            break;
        }
        task_t *t = p->head;
        p->head = t->next;
        if (p->head == NULL) p->tail = NULL;
        pthread_mutex_unlock(&p->lock);

        atomic_store_explicit(&p->busy[id], 1, memory_order_relaxed);
        t->fn(t->arg);
        atomic_store_explicit(&p->busy[id], 0, memory_order_relaxed);
        atomic_fetch_add_explicit(&wstats[id].ntasks, 1,
                                  memory_order_relaxed);  /* completed */
        free(t);
    }
    return NULL;
}

pool_t *pool_create(int nworkers) {
    pool_t *p = malloc(sizeof *p);
    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->not_empty, NULL);
    p->head = p->tail = NULL;
    p->shutdown = false;
    p->nworkers = nworkers;
    p->threads = malloc((size_t)nworkers * sizeof *p->threads);
    p->busy = malloc((size_t)nworkers * sizeof *p->busy);
    for (int i = 0; i < nworkers; i++) {
        atomic_init(&p->busy[i], 0);
        worker_arg_t *wa = malloc(sizeof *wa);
        wa->pool = p; wa->id = i;
        pthread_create(&p->threads[i], NULL, worker_main, wa);
    }
    return p;
}

void pool_submit(pool_t *p, task_fn fn, void *arg) {
    task_t *t = malloc(sizeof *t);
    t->fn = fn; t->arg = arg; t->next = NULL;
    pthread_mutex_lock(&p->lock);
    if (p->tail) p->tail->next = t; else p->head = t;
    p->tail = t;
    pthread_cond_signal(&p->not_empty);
    pthread_mutex_unlock(&p->lock);
}

void pool_destroy(pool_t *p) {
    pthread_mutex_lock(&p->lock);
    p->shutdown = true;
    pthread_cond_broadcast(&p->not_empty);
    pthread_mutex_unlock(&p->lock);
    for (int i = 0; i < p->nworkers; i++)
        pthread_join(p->threads[i], NULL);
    pthread_mutex_destroy(&p->lock);
    pthread_cond_destroy(&p->not_empty);
    free(p->threads);
    free(p->busy);
    free(p);
}

/* ===================== the null instrument ===================== */

typedef struct {
    double   acc;
    unsigned runs;
    char     pad[LINE - sizeof(double) - sizeof(unsigned)];
} cell_t;

typedef struct { long g; cell_t *cell; } job_t;

static job_t *jobs;
static cell_t *cells;

/* dedicated warmup scratch (kept out of the correctness-checked cells) */
#define WARMT 2000
static job_t warm_jobs[WARMT];
static cell_t *warm_cells;

__attribute__((noinline))
static double fma_chain(long g, double seed) {
    double acc = seed, x = 1.0000001, y = 0.0000001;
    for (long k = 0; k < g; k++)
        acc = acc * x + y;
    return acc;
}

static void task_run(void *v) {
    job_t *j = v;
    j->cell->acc = fma_chain(j->g, 1.0);
    j->cell->runs++;
}

/* ===================== timing ===================== */

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

/* sum of completed tasks across workers (relaxed: boundary, not ordering) */
static unsigned long completed(int W) {
    unsigned long s = 0;
    for (int w = 0; w < W; w++)
        s += atomic_load_explicit(&wstats[w].ntasks, memory_order_relaxed);
    return s;
}

/* sleep-poll until 'target' tasks have completed. Sleeping (20us), not
   spinning: a spinning poller would occupy a core and contaminate W ~ Pcores. */
static void wait_completed(int W, unsigned long target) {
    struct timespec nap = {0, 20000};
    while (completed(W) < target)
        nanosleep(&nap, NULL);
}

static void reset_wstats(void) {
    for (int w = 0; w < MAXW; w++) {
        atomic_store_explicit(&wstats[w].ntasks, 0, memory_order_relaxed);
        atomic_store_explicit(&wstats[w].nsleeps, 0, memory_order_relaxed);
    }
}

/* ===================== experiments ===================== */

static void calibrate(void) {
    printf("--- calibration (single-threaded, no pool) ---\n");
    printf("        g       reps      total_ns        ns/task       ns per unit g\n");
    long gs[] = {10, 100, 1000, 10000};
    for (unsigned i = 0; i < sizeof gs / sizeof *gs; i++) {
        long g = gs[i];
        long reps = 20000000L / (g + 20);
        if (reps < 100) reps = 100;
        volatile double sink;
        double s = 1.0;
        double t0 = now_s();
        for (long r = 0; r < reps; r++) s = fma_chain(g, s);
        double t1 = now_s();
        sink = s; (void)sink;
        double total_ns = (t1 - t0) * 1e9;
        double per_task = total_ns / (double)reps;
        printf("%9ld %10ld %13.0f %14.2f %19.4f\n",
               g, reps, total_ns, per_task, per_task / (double)g);
    }
    printf("\n");
}

/* one measured repeat over an ALREADY-LIVE pool: submit T, wait for T
   completions, clock stops there. Digest of per-worker stats returned. */
static double one_rep(pool_t *p, int W, long T, long g,
                      int *active, double *top, double *sleeps_per_t) {
    memset(cells, 0, (size_t)T * sizeof *cells);
    for (long i = 0; i < T; i++) { jobs[i].g = g; jobs[i].cell = &cells[i]; }
    reset_wstats();

    double t0 = now_s();
    for (long i = 0; i < T; i++) pool_submit(p, task_run, &jobs[i]);
    wait_completed(W, (unsigned long)T);          /* completion boundary */
    double t1 = now_s();

    unsigned long total_sleeps = 0, maxt = 0;
    int act = 0;
    for (int w = 0; w < W; w++) {
        unsigned long n = atomic_load_explicit(&wstats[w].ntasks,
                                               memory_order_relaxed);
        total_sleeps += atomic_load_explicit(&wstats[w].nsleeps,
                                             memory_order_relaxed);
        if (n > maxt) maxt = n;
        if (n * 100 >= (unsigned long)T) act++;   /* >=1% of tasks */
    }
    *active = act;
    *top = (double)maxt / (double)T;
    *sleeps_per_t = (double)total_sleeps / (double)T;

    return (double)T / (t1 - t0);
}

/* full sweep point: pool up (unclocked) -> warmup (unclocked) -> REPS
   measured repeats -> median -> teardown (unclocked) -> correctness check
   (after join: fully synchronized). */
static double sweep_point(int W, long T, long g, int *ok,
                          int *active, double *top, double *sleeps_per_t,
                          double *jit) {
    double thr[REPS];
    int act_r[REPS]; double top_r[REPS], spt_r[REPS];

    pool_t *p = pool_create(W);                   /* OUTSIDE the clock */

    /* warmup: steady-state the pool on scratch cells (unclocked) */
    long wt = WARMT < T ? WARMT : T;
    reset_wstats();
    for (long i = 0; i < wt; i++) {
        warm_jobs[i].g = g; warm_jobs[i].cell = &warm_cells[i];
        pool_submit(p, task_run, &warm_jobs[i]);
    }
    wait_completed(W, (unsigned long)wt);

    for (int r = 0; r < REPS; r++)
        thr[r] = one_rep(p, W, T, g, &act_r[r], &top_r[r], &spt_r[r]);

    pool_destroy(p);                              /* OUTSIDE the clock */

    *ok = 1;                                      /* after join: synced */
    for (long i = 0; i < T; i++)
        if (cells[i].runs != 1) { *ok = 0; break; }

    /* median repeat (sort a copy of throughputs, keep index) */
    int idx[REPS];
    for (int r = 0; r < REPS; r++) idx[r] = r;
    for (int a = 0; a < REPS; a++)
        for (int b = a + 1; b < REPS; b++)
            if (thr[idx[b]] < thr[idx[a]]) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
    int med = idx[REPS / 2];

    double dev = 0.0;
    for (int r = 0; r < REPS; r++) {
        double d = thr[r] > thr[med] ? thr[r] - thr[med] : thr[med] - thr[r];
        if (d > dev) dev = d;
    }
    *jit = 100.0 * dev / thr[med];
    *active = act_r[med]; *top = top_r[med]; *sleeps_per_t = spt_r[med];
    return thr[med];
}

static void run_sweep(const char *label, long T, long g, const char *reads,
                      int dump_dist) {
    int Ws[] = {1, 2, 4, 8, 10, 11, 16, 20, 30, 50};
    printf("--- %s : T=%ld tasks, g=%ld ---\n", label, T, g);
    printf("    %s\n", reads);
    printf("    region: submit->complete only; create/warmup/teardown unclocked; median of %d reps\n", REPS);
    printf("        W     tasks/s        speedup   ns/task   active     top   sleeps/T   jit%%   ok\n");
    double base = 0.0;
    for (unsigned i = 0; i < sizeof Ws / sizeof *Ws; i++) {
        int ok, active;
        double top, spt, jit;
        int W = Ws[i];
        double thr = sweep_point(W, T, g, &ok, &active, &top, &spt, &jit);
        if (i == 0) base = thr;
        printf("%9d %11.3e %14.2fx %9.1f %8d %6.1f%% %10.3f %6.1f   %s\n",
               W, thr, thr / base, 1e9 / thr, active, 100.0 * top, spt, jit,
               ok ? "yes" : "NO <-- BUG");
        /* full distribution at the biggest W (from the LAST repeat's stats —
           wstats still hold the final rep at this point) */
        if (dump_dist && W == 50) {
            printf("    task distribution @ W=50 (tasks per worker, last rep):\n    ");
            for (int w = 0; w < W; w++) {
                printf("%lu ", atomic_load_explicit(&wstats[w].ntasks,
                                                    memory_order_relaxed));
                if ((w + 1) % 10 == 0) printf("\n    ");
            }
            printf("\n");
        }
    }
    printf("\n");
}

/* usage:  ./sweep [small] [A|B|cal]
   No selector: cal + A + B (the original blended run).
   A / B / cal: run ONLY that experiment - so /usr/bin/time -l wraps a single
   experiment and the user/sys/context-switch decomposition is attributable
   to it alone. Selector and "small" compose in either order. */
int main(int argc, char **argv) {
    int small = 0, runA = 1, runB = 1, runcal = 1;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "small") == 0) small = 1;
        else if (strcmp(argv[i], "A") == 0)   { runB = 0; runcal = 0; }
        else if (strcmp(argv[i], "B") == 0)   { runA = 0; runcal = 0; }
        else if (strcmp(argv[i], "cal") == 0) { runA = 0; runB = 0; }
        else { fprintf(stderr, "usage: %s [small] [A|B|cal]\n", argv[0]); return 2; }
    }

    long T_small = small ? 20000 : 500000;
    long T_large = small ? 500   : 4000;

    long Tmax = T_small > T_large ? T_small : T_large;
    jobs = malloc((size_t)Tmax * sizeof *jobs);
    if (posix_memalign((void **)&cells, LINE, (size_t)Tmax * sizeof *cells)) return 1;
    if (posix_memalign((void **)&warm_cells, LINE, WARMT * sizeof *warm_cells)) return 1;

    if (runcal) calibrate();

    if (runA)
        run_sweep("A. dispatch ceiling", T_small, 10,
                  "task << dispatch: measures the LOCK, not the cores", 1);

    if (runB)
        run_sweep("B. parallel efficiency", T_large, 100000,
                  "task >> dispatch: measures the CORES, not the lock", 0);

    free(jobs); free(cells); free(warm_cells);
    return 0;
}