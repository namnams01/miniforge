// cachecliff.c — find the cache-hierarchy "cliffs" on Apple Silicon (works on any macOS/Linux box)
//
//   Latency mode (default): a dependent, randomized pointer-chase. Each load's address comes
//   from the previous load (no memory-level parallelism) and the order is a random permutation
//   (no prefetchable stride), so every access pays the FULL latency of whichever level holds it.
//   Stride == cache-line size, so the resident footprint in bytes == the allocation size; the
//   x-axis IS the working set. Latency is flat while footprint fits a level, then steps up.
//
//   Bandwidth mode (--bw): a sequential sum the hardware prefetcher CAN run ahead of, so you
//   read sustained throughput (GB/s) per level instead of latency. Same cliffs, mirror image.
//
// Build:  clang -O3 -Wall -o cachecliff cachecliff.c
// Run:    ./cachecliff          # latency cliffs (ns/access)
//         ./cachecliff --bw     # bandwidth cliffs (GB/s)
//
// Reading it: the step UP in ns (or DOWN in GB/s) centers near each cache level's capacity.
// On Apple Silicon you may see only THREE plateaus (L1 / L2 / DRAM) because the SLC can be
// smaller than the cluster-shared L2 and gets hidden. The effective cliff often lands ~10-20%
// below the nominal capacity (associativity + conflict misses + metadata smear it).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <pthread/qos.h>
#endif

static volatile void *g_sink;   // defeat dead-code elimination of the chase

// Pin a value live in a register + act as a memory/reorder barrier. Without this the optimizer
// hoists the (side-effect-free) chase OUT of the timed interval and you measure ~0 ns.
#define KEEP(x) __asm__ __volatile__("" : "+r"(x) : : "memory")

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static uint64_t sysctl_u64(const char *name) {
#ifdef __APPLE__
    uint64_t v = 0; size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, NULL, 0) == 0) return v;
#endif
    (void)name;
    return 0;
}

// Fisher-Yates shuffle of [0, n)
static void shuffle(uint32_t *a, uint32_t n) {
    for (uint32_t i = n - 1; i > 0; i--) {
        uint32_t j = (uint32_t)(((uint64_t)rand() * (i + 1)) / ((uint64_t)RAND_MAX + 1));
        uint32_t t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

// One dependent random pointer-chase over `bytes`, stepping by `stride`. Returns ns/access.
static double measure_latency(size_t bytes, size_t stride) {
    size_t n = bytes / stride;
    if (n < 2) return 0.0;

    char *buf = NULL;
    if (posix_memalign((void **)&buf, 16384, n * stride) != 0) { perror("alloc"); exit(1); }
    memset(buf, 0, n * stride);                 // fault in every page up front

    uint32_t *order = malloc(n * sizeof(uint32_t));
    if (!order) { perror("alloc order"); exit(1); }
    for (uint32_t i = 0; i < n; i++) order[i] = i;
    shuffle(order, (uint32_t)n);

    // Link a single Hamiltonian cycle: order[0] -> order[1] -> ... -> order[n-1] -> order[0]
    for (uint32_t k = 0; k < n; k++) {
        char *cur = buf + (size_t)order[k] * stride;
        char *nxt = buf + (size_t)order[(k + 1) % n] * stride;
        *(void **)cur = nxt;
    }

    void *p = buf + (size_t)order[0] * stride;

    // Warm-up (callback to the warming-vs-prefetch distinction): reach steady state in the
    // caches/TLB/predictors before timing. A working set bigger than cache can't be warmed in,
    // so cap the warm pass — that's correct, not lazy.
    size_t warm = (n < 1000000) ? n * 2 : 1000000;
    for (size_t w = 0; w < warm; w++) p = *(void **)p;

    // Self-tuning timed loop: chase in chunks until ~0.3 s elapsed; the dependency chain
    // carries across chunks so timing is uninterrupted.
    const size_t CHUNK = 1u << 20;
    const double TARGET = 3e8;
    double t0 = now_ns(), elapsed = 0.0;
    uint64_t accesses = 0;
    while (elapsed < TARGET) {
        for (size_t i = 0; i < CHUNK; i++) { p = *(void **)p; KEEP(p); }
        accesses += CHUNK;
        elapsed = now_ns() - t0;
    }
    g_sink = p;

    free(order);
    free(buf);
    return elapsed / (double)accesses;          // ns per access
}

// Sequential sum over `bytes` (prefetcher-friendly), many passes. Returns GB/s.
static double measure_bandwidth(size_t bytes) {
    size_t n = bytes / sizeof(uint64_t);
    if (n < 8) return 0.0;

    uint64_t *buf = NULL;
    if (posix_memalign((void **)&buf, 16384, n * sizeof(uint64_t)) != 0) { perror("alloc"); exit(1); }
    for (size_t i = 0; i < n; i++) buf[i] = i;

    volatile uint64_t warm = 0;                 // warm pass
    for (size_t i = 0; i < n; i++) warm += buf[i];

    const double TARGET = 3e8;
    double t0 = now_ns(), elapsed = 0.0;
    uint64_t passes = 0, acc = 0;
    while (elapsed < TARGET) {
        uint64_t local = 0;
        for (size_t i = 0; i < n; i++) local += buf[i];
        KEEP(local); acc += local; passes++;
        elapsed = now_ns() - t0;
    }
    g_sink = (void *)(uintptr_t)acc;

    free(buf);
    double touched = (double)passes * (double)n * sizeof(uint64_t);
    return touched / elapsed;                    // bytes/ns == GB/s
}

static void human(size_t sz, char *out, size_t outsz) {
    if (sz >= (1u << 20)) snprintf(out, outsz, "%zu MiB", sz >> 20);
    else                  snprintf(out, outsz, "%zu KiB", sz >> 10);
}

int main(int argc, char **argv) {
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);  // bias scheduling onto P-cores
#endif
    srand(12345);
    int bw = (argc > 1 && strcmp(argv[1], "--bw") == 0);

    uint64_t line = sysctl_u64("hw.cachelinesize");
    uint64_t l1d  = sysctl_u64("hw.perflevel0.l1dcachesize"); if (!l1d) l1d = sysctl_u64("hw.l1dcachesize");
    uint64_t l2   = sysctl_u64("hw.perflevel0.l2cachesize");  if (!l2)  l2  = sysctl_u64("hw.l2cachesize");
    uint64_t l3   = sysctl_u64("hw.l3cachesize");
    uint64_t mem  = sysctl_u64("hw.memsize");
    if (!line) line = 128;                       // Apple Silicon P-core line; safe default elsewhere

    printf("# detected hierarchy (via sysctl; 0/blank = not exposed):\n");
    printf("#   cache line : %llu B\n", (unsigned long long)line);
    if (l1d) printf("#   L1d        : %llu KiB    (predict first cliff near here)\n",  (unsigned long long)(l1d >> 10));
    if (l2)  printf("#   L2         : %llu KiB    (predict next cliff near here)\n",   (unsigned long long)(l2  >> 10));
    if (l3)  printf("#   L3         : %llu KiB\n", (unsigned long long)(l3 >> 10));
    else     printf("#   L3         : none — Apple Silicon uses a shared SLC on the memory fabric,\n"
                    "#                not exposed by sysctl and sometimes smaller than L2 (so it may be hidden)\n");
    if (mem) printf("#   DRAM       : %llu MiB\n", (unsigned long long)(mem >> 20));
    printf("#   stride     : %llu B  (== line, so footprint == allocation size)\n",
           (unsigned long long)line);
    printf("#   mode       : %s\n\n", bw ? "bandwidth (sequential, prefetch ON)"
                                         : "latency (random pointer-chase, prefetch defeated)");

    size_t stride = (size_t)line;
    if (argc > 2) stride = (size_t)strtoull(argv[2], NULL, 10);
    size_t cap = 512ull * 1024 * 1024;
    if (mem && (mem / 4) < cap) cap = (size_t)(mem / 4);

    printf("%12s %14s\n", "footprint", bw ? "GB/s" : "ns/access");
    printf("%12s %14s\n", "---------", "---------");

    double prev = 0.0;
    size_t start = stride * 4;
    for (size_t sz = start; sz <= cap; sz <<= 1) {     // 8 KiB .. cap, doubling
        double v = bw ? measure_bandwidth(sz) : measure_latency(sz, stride);
        const char *flag = "";
        if (prev > 0.0) {
            double ratio = bw ? (prev / v) : (v / prev);     // slower latency / lower bandwidth
            if (ratio > 1.4) flag = "   <== CLIFF";
        }
        char hr[32]; human(sz, hr, sizeof hr);
        printf("%12s %14.2f%s\n", hr, v, flag);
        size_t nodes = sz / stride;
        size_t cache_footprint = nodes * (size_t)line;   // bytes of lines actually resident
        prev = v;
    }

    printf("\n# the step up in ns (or down in GB/s) marks each level; its midpoint ~ that level's\n"
           "# capacity. effective cliff usually lands a bit under nominal (associativity smear).\n");
    (void)g_sink;
    return 0;
}