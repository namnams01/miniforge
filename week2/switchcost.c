// switchcost.c — Week 2: the isolated switch-cost microbenchmark.
// Measures, EMPIRICALLY on this machine, the cost of the kernel park/wake
// round trip that dispatch A's sys-time decomposition attributed the cliff to.
//
//   cc -O2 -Wall -Wextra switchcost.c -o switchcost
//   /usr/bin/time -l ./switchcost
//
// [Measurement scaffolding — Claude, at N's request; logged per AI_PROVENANCE.
//  The experiment design (forced-alternation ping-pong on the SAME primitive
//  the pool uses) is the outstanding component-level measurement from the
//  Week-2 list. Constants produced here replace the 23.84 ns ledger entry,
//  which was measured at a narrower boundary than it was used for.]
//
// Three tiers, so the number decomposes instead of arriving as a blob:
//
//   (1) uncontended  — lock/unlock with no waiter: the userspace fast path.
//                      Should be tens of ns; NO kernel involvement.
//   (2) syscall floor — a minimal real syscall (getppid): the bare trap
//                      boundary, round trip user->kernel->user. This is the
//                      class of number the old 23.84 ns constant lived in.
//   (3) ping-pong    — two threads forced to strictly alternate through one
//                      mutex + two condvars. Every iteration is: signal the
//                      other side, park self, get woken. One iteration = TWO
//                      park/wake handoffs (one per thread). Reported both as
//                      ns/round-trip and ns/handoff. THIS is the constant the
//                      plateau arithmetic needs.
//
// Discipline: warmup, R repeats, median + max-deviation jitter, CLOCK_MONOTONIC.
// macOS caveats (standing rules): no core pinning or clock locking available;
// QoS/scheduler may migrate threads (P vs E cores) — run plugged in, quiet
// machine, and the number is macOS-provisional until the Week-3.5 Linux
// replication. Expect cross-core vs same-core handoffs to differ; the spread
// across repeats is partly THAT, which is honest — the pool's workers live
// under the same scheduler.

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define REPS 9          /* repeats per experiment; median reported */

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static int cmp_d(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void report(const char *label, double *ns, int n, double per_div,
                   const char *unit) {
    qsort(ns, (size_t)n, sizeof *ns, cmp_d);
    double med = ns[n / 2];
    double dev = 0.0;
    for (int i = 0; i < n; i++) {
        double d = ns[i] > med ? ns[i] - med : med - ns[i];
        if (d > dev) dev = d;
    }
    printf("%-34s median %10.1f ns/%s   (jit %5.1f%%, %d reps)\n",
           label, med / per_div, unit, 100.0 * dev / med, n);
}

/* ===================== (1) uncontended lock/unlock ===================== */

static void bench_uncontended(void) {
    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    const long N = 2000000;
    double ns[REPS];
    /* warmup */
    for (long i = 0; i < 100000; i++) { pthread_mutex_lock(&m); pthread_mutex_unlock(&m); }
    for (int r = 0; r < REPS; r++) {
        double t0 = now_s();
        for (long i = 0; i < N; i++) { pthread_mutex_lock(&m); pthread_mutex_unlock(&m); }
        double t1 = now_s();
        ns[r] = (t1 - t0) * 1e9 / (double)N;
    }
    report("(1) uncontended lock+unlock", ns, REPS, 1.0, "pair");
}

/* ===================== (2) syscall floor ===================== */

static void bench_syscall(void) {
    const long N = 500000;
    double ns[REPS];
    volatile pid_t sink;
    for (long i = 0; i < 10000; i++) sink = getppid();
    for (int r = 0; r < REPS; r++) {
        double t0 = now_s();
        for (long i = 0; i < N; i++) sink = getppid();
        double t1 = now_s();
        (void)sink;
        ns[r] = (t1 - t0) * 1e9 / (double)N;
    }
    report("(2) syscall floor (getppid)", ns, REPS, 1.0, "call");
}

/* ===================== (3) condvar ping-pong ===================== */
/* Strict alternation: turn=0 means it is PING's turn, turn=1 PONG's turn.
   Each side: wait for its turn, flip the turn, signal the other side's cv.
   Two condvars so a signal can never be consumed by the signaler itself.
   Every iteration parks each thread exactly once (after warmup, the partner
   is always already waiting, so every handoff takes the kernel path). */

typedef struct {
    pthread_mutex_t m;
    pthread_cond_t cv[2];       /* cv[side]: signaled when it becomes side's turn */
    int turn;
    long rounds;                /* iterations per timed repeat */
    bool run;                   /* pong loops until told to stop */
} pp_t;

static pp_t pp = { PTHREAD_MUTEX_INITIALIZER,
                   { PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER },
                   0, 0, true };

static void *pong_main(void *v) {
    (void)v;
    pthread_mutex_lock(&pp.m);
    for (;;) {
        while (pp.turn != 1 && pp.run)
            pthread_cond_wait(&pp.cv[1], &pp.m);
        if (!pp.run) break;
        pp.turn = 0;
        pthread_cond_signal(&pp.cv[0]);
    }
    pthread_mutex_unlock(&pp.m);
    return NULL;
}

static double pingpong_once(long rounds) {
    /* runs on the PING side; pong thread is already parked on cv[1] */
    double t0 = now_s();
    pthread_mutex_lock(&pp.m);
    for (long i = 0; i < rounds; i++) {
        pp.turn = 1;
        pthread_cond_signal(&pp.cv[1]);
        while (pp.turn != 0)
            pthread_cond_wait(&pp.cv[0], &pp.m);
    }
    pthread_mutex_unlock(&pp.m);
    double t1 = now_s();
    return (t1 - t0) * 1e9 / (double)rounds;   /* ns per ROUND TRIP */
}

static void bench_pingpong(void) {
    const long ROUNDS = 200000;
    double ns[REPS];
    pthread_t th;
    pthread_create(&th, NULL, pong_main, NULL);

    (void)pingpong_once(20000);                 /* warmup, unrecorded */
    for (int r = 0; r < REPS; r++)
        ns[r] = pingpong_once(ROUNDS);

    /* stop pong */
    pthread_mutex_lock(&pp.m);
    pp.run = false;
    pthread_cond_signal(&pp.cv[1]);
    pthread_mutex_unlock(&pp.m);
    pthread_join(th, NULL);

    report("(3) ping-pong round trip", ns, REPS, 1.0, "round-trip");
    report("    -> per park/wake handoff", ns, REPS, 2.0, "handoff");
}

int main(void) {
    printf("switch-cost microbenchmark - macOS-provisional (no pinning, no clock lock)\n");
    printf("run plugged in, machine quiet; W3.5 Linux replication supersedes\n\n");
    bench_uncontended();
    bench_syscall();
    bench_pingpong();
    printf("\nledger guidance: (3)/handoff is the constant the dispatch-A plateau\n"
           "arithmetic needs (label: measured, this file, date). (2) is the class the\n"
           "old 23.84 ns entry belonged to. (1) confirms the fast path stays in\n"
           "userspace. Cross-check: dispatch A measured ~11.1 us of sys CPU per switch\n"
           "event (333.6 s / 30.1M); (3) measures the WALL round trip - they need not\n"
           "match exactly (sys CPU sums across cores; wall does not), but they must\n"
           "agree in order of magnitude for the story to close.\n");
    return 0;
}