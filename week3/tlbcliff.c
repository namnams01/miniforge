#define _GNU_SOURCE

// tlbcliff.c — empirical TLB-capacity and page-walk-locality cliffs
//
// Capacity mode (default):
//   Compare two dependent randomized pointer chases with the SAME number of
//   live cache lines:
//
//     compact: one node per cache line
//     paged:   one node per VM page
//
//   paged - compact is a much cleaner estimate of translation pressure than
//   the raw paged latency, because both chains perform the same number of
//   dependent loads and keep the same number of data-cache lines resident.
//
// Sparse-walk mode (--walk):
//   Keep page count fixed, but place those pages farther apart in virtual
//   address space. This changes sharing/locality in the page-table hierarchy
//   while keeping the number of data lines and TLB-demanding pages constant.
//   Steps near the printed page-table coverage boundaries are evidence about
//   page-walk cache/table locality. They are NOT, by themselves, a proof of
//   the exact architectural page-table depth.
//
// macOS note:
//   THREAD_AFFINITY_POLICY is only an affinity-group scheduler hint; it is not
//   true CPU pinning. USER_INTERACTIVE QoS biases Apple Silicon scheduling
//   toward performance cores, but neither API prevents migration.
//
// Build:
//   macOS: clang -O3 -std=c11 -Wall -Wextra -o tlbcliff tlbcliff.c
//   Linux: clang -O3 -std=c11 -Wall -Wextra -o tlbcliff tlbcliff.c
//
// Run:
//   ./tlbcliff
//   ./tlbcliff --walk
//   ./tlbcliff --deep-walk
//   ./tlbcliff --all --target-ms 200 --trials 7
//   ./tlbcliff --geometry
//
// Useful knobs:
//   --max-pages N       largest capacity-test working set, in pages
//   --walk-pages N      fixed page count in sparse-walk mode
//   --max-span-gib N    maximum reserved VA span in sparse-walk mode
//   --target-ms N       time per sample
//   --trials N          odd number of paired samples
//   --cpu N             real pinning on Linux; ignored on macOS

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <sys/sysctl.h>
#else
#include <sched.h>
#endif

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

static void *volatile g_sink;

#define KEEP(x) __asm__ __volatile__("" : "+r"(x) : : "memory")

typedef enum {
    MODE_CAPACITY,
    MODE_WALK,
    MODE_ALL,
    MODE_GEOMETRY
} mode_tlb_t;

typedef struct {
    mode_tlb_t mode;
    size_t max_pages;
    size_t walk_pages;
    size_t max_span;
    double target_ns;
    int trials;
    int cpu;
    uint64_t seed;
} options_t;

typedef struct {
    void *mapping;
    size_t mapping_size;
    size_t nodes;
    void *head;
} chain_t;

typedef struct {
    double median;
    double minimum;
} stats_t;

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static double now_ns(void) {
#ifdef __APPLE__
    return (double)clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
#else
    struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
#endif
}

#ifdef __APPLE__
static uint64_t sysctl_u64(const char *name) {
    uint64_t value = 0;
    size_t len = sizeof(value);
    if (sysctlbyname(name, &value, &len, NULL, 0) == 0) return value;
    return 0;
}
#endif

static size_t system_page_size(void) {
    long p = sysconf(_SC_PAGESIZE);
    if (p <= 0) die("sysconf(_SC_PAGESIZE)");
    return (size_t)p;
}

static size_t system_cache_line_size(void) {
#ifdef __APPLE__
    uint64_t line = sysctl_u64("hw.cachelinesize");
    if (line >= sizeof(void *)) return (size_t)line;
#else
#ifdef _SC_LEVEL1_DCACHE_LINESIZE
    long line = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    if (line >= (long)sizeof(void *)) return (size_t)line;
#endif
#endif
    return 64;
}

static uint64_t system_memory_size(void) {
#ifdef __APPLE__
    return sysctl_u64("hw.memsize");
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    if (pages <= 0 || page_size <= 0) return 0;
    return (uint64_t)pages * (uint64_t)page_size;
#endif
}

static int is_power_of_two_u64(uint64_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

static unsigned ilog2_u64(uint64_t x) {
    unsigned result = 0;
    while (x > 1) {
        x >>= 1;
        result++;
    }
    return result;
}

static int checked_mul_size(size_t a, size_t b, size_t *out) {
    if (a != 0 && b > SIZE_MAX / a) return 0;
    *out = a * b;
    return 1;
}

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

static void shuffle_u32(uint32_t *a, size_t n, uint64_t seed) {
    if (n < 2) return;
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = (size_t)(splitmix64(&seed) % (i + 1));
        uint32_t tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
    }
}

static void human_bytes(uint64_t bytes, char *out, size_t out_size) {
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double value = (double)bytes;
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0;
        unit++;
    }
    if (value >= 100.0 || unit == 0) snprintf(out, out_size, "%.0f %s", value, units[unit]);
    else if (value >= 10.0) snprintf(out, out_size, "%.1f %s", value, units[unit]);
    else snprintf(out, out_size, "%.2f %s", value, units[unit]);
}

static uint64_t parse_u64(const char *text, const char *name) {
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        fprintf(stderr, "invalid %s: %s\n", name, text);
        exit(EXIT_FAILURE);
    }
    return (uint64_t)value;
}

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s [--capacity|--walk|--deep-walk|--all|--geometry]\n"
            "          [--max-pages N] [--walk-pages N] [--max-span-gib N]\n"
            "          [--target-ms N] [--trials N] [--cpu N] [--seed N]\n",
            program);
}

static options_t parse_options(int argc, char **argv) {
    options_t opt = {
        .mode = MODE_CAPACITY,
        .max_pages = 16384,
        .walk_pages = 1024,
        .max_span = (size_t)1 << 40,  // 1 TiB of VA, not physical memory
        .target_ns = 100e6,
        .trials = 5,
        .cpu = -1,
        .seed = UINT64_C(0x4d595df4d0f33173)
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--capacity") == 0) {
            opt.mode = MODE_CAPACITY;
        } else if (strcmp(argv[i], "--walk") == 0) {
            opt.mode = MODE_WALK;
        } else if (strcmp(argv[i], "--deep-walk") == 0) {
            opt.mode = MODE_WALK;
            opt.walk_pages = 512;
            opt.max_span = (size_t)32 << 40;  // attempts up to a 32-TiB sparse arena
        } else if (strcmp(argv[i], "--all") == 0) {
            opt.mode = MODE_ALL;
        } else if (strcmp(argv[i], "--geometry") == 0) {
            opt.mode = MODE_GEOMETRY;
        } else if (strcmp(argv[i], "--max-pages") == 0 && i + 1 < argc) {
            opt.max_pages = (size_t)parse_u64(argv[++i], "max pages");
        } else if (strcmp(argv[i], "--walk-pages") == 0 && i + 1 < argc) {
            opt.walk_pages = (size_t)parse_u64(argv[++i], "walk pages");
        } else if (strcmp(argv[i], "--max-span-gib") == 0 && i + 1 < argc) {
            uint64_t gib = parse_u64(argv[++i], "max span GiB");
            if (gib > SIZE_MAX / (UINT64_C(1) << 30)) {
                fprintf(stderr, "max span is too large for size_t\n");
                exit(EXIT_FAILURE);
            }
            opt.max_span = (size_t)(gib << 30);
        } else if (strcmp(argv[i], "--target-ms") == 0 && i + 1 < argc) {
            uint64_t ms = parse_u64(argv[++i], "target milliseconds");
            if (ms == 0) {
                fprintf(stderr, "target milliseconds must be positive\n");
                exit(EXIT_FAILURE);
            }
            opt.target_ns = (double)ms * 1e6;
        } else if (strcmp(argv[i], "--trials") == 0 && i + 1 < argc) {
            opt.trials = (int)parse_u64(argv[++i], "trials");
        } else if (strcmp(argv[i], "--cpu") == 0 && i + 1 < argc) {
            opt.cpu = (int)parse_u64(argv[++i], "CPU");
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            opt.seed = parse_u64(argv[++i], "seed");
        } else {
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (opt.max_pages < 4 || opt.walk_pages < 2) {
        fprintf(stderr, "page counts are too small\n");
        exit(EXIT_FAILURE);
    }
    if (opt.trials < 1 || opt.trials > 31 || (opt.trials % 2) == 0) {
        fprintf(stderr, "--trials must be an odd number from 1 through 31\n");
        exit(EXIT_FAILURE);
    }
    return opt;
}

static void configure_benchmark_thread(int cpu) {
#ifdef __APPLE__
    int qerr = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    if (qerr != 0) fprintf(stderr, "# warning: QoS request failed: %d\n", qerr);

    thread_affinity_policy_data_t policy = {.affinity_tag = 1};
    kern_return_t kr = thread_policy_set(
        pthread_mach_thread_np(pthread_self()),
        THREAD_AFFINITY_POLICY,
        (thread_policy_t)&policy,
        THREAD_AFFINITY_POLICY_COUNT);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "# warning: THREAD_AFFINITY_POLICY failed: %d\n", kr);
    }
    if (cpu >= 0) {
        fprintf(stderr, "# warning: macOS has no public true CPU-pinning API; --cpu ignored\n");
    }
#else
    if (cpu >= 0) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpu, &set);
        if (sched_setaffinity(0, sizeof(set), &set) != 0) die("sched_setaffinity");
    }
#endif
}

__attribute__((noinline))
static void *pointer_chase(void *p, uint64_t accesses) {
    for (uint64_t i = 0; i < accesses; i++) {
        p = *(void *volatile *)p;
    }
    KEEP(p);
    return p;
}

static void destroy_chain(chain_t *chain) {
    if (chain->mapping && chain->mapping != MAP_FAILED) {
        if (munmap(chain->mapping, chain->mapping_size) != 0) die("munmap");
    }
    memset(chain, 0, sizeof(*chain));
}

static int allocate_order(uint32_t **order_out, size_t n, uint64_t seed) {
    if (n > UINT32_MAX) return 0;
    uint32_t *order = malloc(n * sizeof(*order));
    if (!order) return 0;
    for (size_t i = 0; i < n; i++) order[i] = (uint32_t)i;
    shuffle_u32(order, n, seed);
    *order_out = order;
    return 1;
}

// compact=1: one node per cache line. compact=0: one node per VM page.
static int make_regular_chain(chain_t *out,
                              size_t n,
                              size_t page_size,
                              size_t line_size,
                              int compact,
                              uint64_t seed) {
    memset(out, 0, sizeof(*out));
    size_t stride = compact ? line_size : page_size;
    size_t mapping_size;
    if (!checked_mul_size(n, stride, &mapping_size)) return 0;

    char *mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapping == MAP_FAILED) return 0;

#ifdef MADV_RANDOM
    (void)madvise(mapping, mapping_size, MADV_RANDOM);
#endif

    uint32_t *order = NULL;
    void **nodes = NULL;
    if (!allocate_order(&order, n, seed)) goto fail;
    nodes = malloc(n * sizeof(*nodes));
    if (!nodes) goto fail;

    size_t lines_per_page = page_size / line_size;
    if (lines_per_page == 0) lines_per_page = 1;

    for (size_t i = 0; i < n; i++) {
        size_t offset = 0;
        if (!compact) {
            // Rotate the in-page line so all pages do not alias at offset zero.
            offset = ((i * 131u) % lines_per_page) * line_size;
        }
        nodes[i] = mapping + i * stride + offset;
    }

    for (size_t k = 0; k < n; k++) {
        void *current = nodes[order[k]];
        void *next = nodes[order[(k + 1) % n]];
        *(void **)current = next;
    }

    out->mapping = mapping;
    out->mapping_size = mapping_size;
    out->nodes = n;
    out->head = nodes[order[0]];
    free(nodes);
    free(order);
    return 1;

fail:
    free(nodes);
    free(order);
    munmap(mapping, mapping_size);
    return 0;
}

// Reserve n disjoint buckets of width gap. Activate one randomized page inside
// each bucket. The number of touched pages is fixed while the VA span changes.
static int make_sparse_chain(chain_t *out,
                             size_t n,
                             size_t gap,
                             size_t page_size,
                             size_t line_size,
                             uint64_t seed) {
    memset(out, 0, sizeof(*out));
    if (gap < page_size || gap % page_size != 0) return 0;

    size_t span;
    if (!checked_mul_size(n, gap, &span)) return 0;

    int flags = MAP_PRIVATE | MAP_ANON;
#ifdef MAP_NORESERVE
    flags |= MAP_NORESERVE;
#endif
    char *arena = mmap(NULL, span, PROT_NONE, flags, -1, 0);
    if (arena == MAP_FAILED) return 0;

    uint32_t *order = NULL;
    void **nodes = NULL;
    if (!allocate_order(&order, n, seed)) goto fail;
    nodes = malloc(n * sizeof(*nodes));
    if (!nodes) goto fail;

    size_t pages_per_bucket = gap / page_size;
    size_t lines_per_page = page_size / line_size;
    if (lines_per_page == 0) lines_per_page = 1;

    uint64_t hash_state = seed ^ UINT64_C(0xa0761d6478bd642f);
    for (size_t i = 0; i < n; i++) {
        uint64_t r = splitmix64(&hash_state);
        size_t page_slot = pages_per_bucket > 1 ? (size_t)(r % pages_per_bucket) : 0;
        char *page = arena + i * gap + page_slot * page_size;

        if (mprotect(page, page_size, PROT_READ | PROT_WRITE) != 0) goto fail;

        size_t line_slot = (size_t)((r >> 32) % lines_per_page);
        void *node = page + line_slot * line_size;
        *(volatile unsigned char *)node = 0;  // demand-fault this page now
        nodes[i] = node;
    }

    for (size_t k = 0; k < n; k++) {
        void *current = nodes[order[k]];
        void *next = nodes[order[(k + 1) % n]];
        *(void **)current = next;
    }

    out->mapping = arena;
    out->mapping_size = span;
    out->nodes = n;
    out->head = nodes[order[0]];
    free(nodes);
    free(order);
    return 1;

fail:
    free(nodes);
    free(order);
    munmap(arena, span);
    return 0;
}

static double time_chain_sample(void **state,
                                size_t nodes,
                                double target_ns) {
    uint64_t warm = (uint64_t)nodes * 2;
    if (warm > UINT64_C(1000000)) warm = UINT64_C(1000000);
    if (warm < 1024) warm = 1024;
    *state = pointer_chase(*state, warm);

    const uint64_t chunk = UINT64_C(1) << 18;
    uint64_t accesses = 0;
    double start = now_ns();
    double elapsed = 0.0;
    do {
        *state = pointer_chase(*state, chunk);
        accesses += chunk;
        elapsed = now_ns() - start;
    } while (elapsed < target_ns);

    g_sink = *state;
    return elapsed / (double)accesses;
}

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left;
    double b = *(const double *)right;
    return (a > b) - (a < b);
}

static stats_t summarize(double *samples, int count) {
    qsort(samples, (size_t)count, sizeof(samples[0]), compare_double);
    stats_t stats = {
        .median = samples[count / 2],
        .minimum = samples[0]
    };
    return stats;
}

static void measure_pair(chain_t *control,
                         chain_t *test,
                         double target_ns,
                         int trials,
                         stats_t *control_stats,
                         stats_t *test_stats) {
    double *control_samples = malloc((size_t)trials * sizeof(*control_samples));
    double *test_samples = malloc((size_t)trials * sizeof(*test_samples));
    if (!control_samples || !test_samples) die("malloc samples");

    void *control_state = control->head;
    void *test_state = test->head;

    // Initial steady-state pass.
    control_state = pointer_chase(control_state, (uint64_t)control->nodes * 2);
    test_state = pointer_chase(test_state, (uint64_t)test->nodes * 2);

    for (int trial = 0; trial < trials; trial++) {
        if ((trial & 1) == 0) {
            control_samples[trial] = time_chain_sample(
                &control_state, control->nodes, target_ns);
            test_samples[trial] = time_chain_sample(
                &test_state, test->nodes, target_ns);
        } else {
            test_samples[trial] = time_chain_sample(
                &test_state, test->nodes, target_ns);
            control_samples[trial] = time_chain_sample(
                &control_state, control->nodes, target_ns);
        }
    }

    *control_stats = summarize(control_samples, trials);
    *test_stats = summarize(test_samples, trials);
    free(control_samples);
    free(test_samples);
}

static unsigned levels_for_va_bits(unsigned va_bits,
                                   unsigned page_bits,
                                   unsigned index_bits) {
    if (va_bits <= page_bits) return 0;
    unsigned translated = va_bits - page_bits;
    return (translated + index_bits - 1) / index_bits;
}

static void print_geometry(size_t page_size) {
    const uint64_t pte_size = 8;  // AArch64 and x86-64 translation entries
    uint64_t entries = page_size / pte_size;

    printf("# page-table geometry inferred from page size and 8-byte entries\n");
    printf("#   VM page size       : %zu B\n", page_size);
    printf("#   entries/table page : %" PRIu64 "\n", entries);

    if (!is_power_of_two_u64(page_size) || !is_power_of_two_u64(entries)) {
        printf("#   non-power-of-two geometry: cannot derive index widths cleanly\n");
        return;
    }

    unsigned page_bits = ilog2_u64(page_size);
    unsigned index_bits = ilog2_u64(entries);
    printf("#   page-offset bits   : %u\n", page_bits);
    printf("#   index bits/level   : %u\n", index_bits);

    uint64_t coverage = page_size;
    for (unsigned level = 1; level <= 4; level++) {
        if (coverage > UINT64_MAX / entries) break;
        coverage *= entries;
        char hr[32];
        human_bytes(coverage, hr, sizeof(hr));
        if (level == 1) {
            printf("#   one leaf table maps: %s\n", hr);
        } else {
            printf("#   +%u upper level maps: %s\n", level - 1, hr);
        }
    }

    const unsigned candidates[] = {39, 42, 47, 48, 52, 57};
    printf("#\n# candidate translation depths (formula only; not an OS query):\n");
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        unsigned bits = candidates[i];
        printf("#   %2u-bit VA -> %u indexed level(s) above the page offset\n",
               bits, levels_for_va_bits(bits, page_bits, index_bits));
    }
    printf("#\n# Sparse-walk timing can reveal reuse boundaries between table pages,\n"
           "# but exact active root depth requires privileged counters/registers or\n"
           "# authoritative kernel configuration. Do not infer depth from one cliff.\n");
}

static size_t next_capacity_pages(size_t pages) {
    size_t step = pages / 4;
    if (step < 1) step = 1;
    if (pages > SIZE_MAX - step) return SIZE_MAX;
    return pages + step;
}

static void run_capacity(const options_t *opt,
                         size_t page_size,
                         size_t line_size) {
    printf("\n# CAPACITY MODE\n");
    printf("# compact and paged chains contain the same number of cache lines.\n");
    printf("# delta = paged - compact; a translation cliff should raise paged/delta\n"
           "# without a matching compact-chain cliff.\n");
    printf("%9s %13s %12s %12s %12s %9s %s\n",
           "pages", "VA coverage", "compact ns", "paged ns", "delta ns", "ratio", "note");
    printf("%9s %13s %12s %12s %12s %9s %s\n",
           "-----", "-----------", "----------", "--------", "--------", "-----", "----");

    double previous_paged = 0.0;
    double previous_compact = 0.0;

    for (size_t pages = 4; pages <= opt->max_pages; pages = next_capacity_pages(pages)) {
        chain_t compact, paged;
        uint64_t seed = opt->seed ^ (uint64_t)pages;
        if (!make_regular_chain(&compact, pages, page_size, line_size, 1, seed)) {
            die("make compact chain");
        }
        if (!make_regular_chain(&paged, pages, page_size, line_size, 0, seed)) {
            destroy_chain(&compact);
            die("make paged chain");
        }

        stats_t compact_stats, paged_stats;
        measure_pair(&compact, &paged, opt->target_ns, opt->trials,
                     &compact_stats, &paged_stats);

        uint64_t coverage = (uint64_t)pages * (uint64_t)page_size;
        char hr[32];
        human_bytes(coverage, hr, sizeof(hr));
        double delta = paged_stats.median - compact_stats.median;
        double ratio = compact_stats.median > 0.0
                           ? paged_stats.median / compact_stats.median
                           : 0.0;

        const char *note = "";
        if (previous_paged > 0.0 && previous_compact > 0.0) {
            double paged_step = paged_stats.median / previous_paged;
            double compact_step = compact_stats.median / previous_compact;
            if (paged_step > 1.18 && compact_step < 1.10) {
                note = "<== candidate TLB cliff";
            } else if (paged_step > 1.18 && compact_step >= 1.10) {
                note = "<== shared cache/system cliff";
            }
        }

        printf("%9zu %13s %12.3f %12.3f %12.3f %9.3f %s\n",
               pages, hr, compact_stats.median, paged_stats.median,
               delta, ratio, note);
        fflush(stdout);

        previous_paged = paged_stats.median;
        previous_compact = compact_stats.median;
        destroy_chain(&paged);
        destroy_chain(&compact);

        if (pages == SIZE_MAX) break;
    }

    printf("#\n# Use the MEDIAN curve to locate knees, then inspect repeated runs and the\n"
           "# minimum/lower envelope if macOS migration or interrupts add outliers.\n");
}

static const char *geometry_note_for_gap(uint64_t gap,
                                         uint64_t page_size) {
    uint64_t entries = page_size / 8;
    uint64_t coverage = page_size;
    static char note[80];
    note[0] = '\0';

    for (unsigned i = 1; i <= 4; i++) {
        if (coverage > UINT64_MAX / entries) break;
        coverage *= entries;
        if (gap == coverage) {
            if (i == 1) return "<== one leaf-table coverage";
            snprintf(note, sizeof(note), "<== +%u upper-level coverage", i - 1);
            return note;
        }
    }
    return "";
}

static void run_walk(const options_t *opt,
                     size_t page_size,
                     size_t line_size) {
    printf("\n# SPARSE PAGE-WALK LOCALITY MODE\n");
    printf("# fixed pages=%zu; only virtual spacing/span changes.\n", opt->walk_pages);
    printf("# Each sparse point reserves PROT_NONE VA and activates one randomized page\n"
           "# per gap-sized bucket. Reservation consumes address space, not gap*pages\n"
           "# of physical memory. A failed huge reservation ends the sweep.\n");

    print_geometry(page_size);

    chain_t compact;
    if (!make_regular_chain(&compact, opt->walk_pages, page_size, line_size,
                            1, opt->seed ^ UINT64_C(0x1111111111111111))) {
        die("make compact walk control");
    }

    size_t max_gap = opt->max_span / opt->walk_pages;
    max_gap -= max_gap % page_size;
    if (max_gap < page_size) {
        fprintf(stderr, "max span is too small for walk page count\n");
        destroy_chain(&compact);
        return;
    }

    printf("%13s %13s %12s %12s %12s %9s %s\n",
           "page gap", "VA span", "compact ns", "sparse ns", "delta ns", "ratio", "boundary");
    printf("%13s %13s %12s %12s %12s %9s %s\n",
           "--------", "-------", "----------", "---------", "--------", "-----", "--------");

    for (size_t gap = page_size; gap <= max_gap; gap <<= 1) {
        size_t span;
        if (!checked_mul_size(opt->walk_pages, gap, &span)) break;

        chain_t sparse;
        uint64_t seed = opt->seed ^ (uint64_t)gap;
        if (!make_sparse_chain(&sparse, opt->walk_pages, gap,
                               page_size, line_size, seed)) {
            char span_hr[32];
            human_bytes((uint64_t)span, span_hr, sizeof(span_hr));
            fprintf(stderr,
                    "# sparse reservation/setup failed at span=%s: %s; stopping\n",
                    span_hr, strerror(errno));
            break;
        }

        stats_t compact_stats, sparse_stats;
        measure_pair(&compact, &sparse, opt->target_ns, opt->trials,
                     &compact_stats, &sparse_stats);

        char gap_hr[32], span_hr[32];
        human_bytes((uint64_t)gap, gap_hr, sizeof(gap_hr));
        human_bytes((uint64_t)span, span_hr, sizeof(span_hr));
        double delta = sparse_stats.median - compact_stats.median;
        double ratio = sparse_stats.median / compact_stats.median;

        printf("%13s %13s %12.3f %12.3f %12.3f %9.3f %s\n",
               gap_hr, span_hr, compact_stats.median, sparse_stats.median,
               delta, ratio, geometry_note_for_gap((uint64_t)gap,
                                                   (uint64_t)page_size));
        fflush(stdout);
        destroy_chain(&sparse);

        if (gap > max_gap / 2) break;
    }

    destroy_chain(&compact);
    printf("#\n# Interpretation: a rise around 'one leaf-table coverage' suggests that\n"
           "# successive translations stopped sharing the same leaf table page. Later\n"
           "# boundaries test sharing of higher table pages. Page-walk caches and cache\n"
           "# associativity can move or hide these knees.\n");
}

int main(int argc, char **argv) {
    options_t opt = parse_options(argc, argv);
    configure_benchmark_thread(opt.cpu);

    size_t page_size = system_page_size();
    size_t line_size = system_cache_line_size();
    uint64_t memory = system_memory_size();

    if (page_size < sizeof(void *) || line_size < sizeof(void *) ||
        page_size % line_size != 0) {
        fprintf(stderr, "unsupported page/cache-line geometry: page=%zu line=%zu\n",
                page_size, line_size);
        return EXIT_FAILURE;
    }

    // Avoid accidentally touching an unreasonable fraction of RAM in capacity mode.
    if (memory) {
        uint64_t memory_limited_pages = memory / 8 / page_size;
        if (memory_limited_pages >= 4 && opt.max_pages > memory_limited_pages) {
            opt.max_pages = (size_t)memory_limited_pages;
        }
    }

    char page_hr[32], line_hr[32], mem_hr[32], span_hr[32];
    human_bytes((uint64_t)page_size, page_hr, sizeof(page_hr));
    human_bytes((uint64_t)line_size, line_hr, sizeof(line_hr));
    human_bytes(memory, mem_hr, sizeof(mem_hr));
    human_bytes((uint64_t)opt.max_span, span_hr, sizeof(span_hr));

    printf("# tlbcliff configuration\n");
    printf("#   page size      : %s\n", page_hr);
    printf("#   cache line     : %s\n", line_hr);
    if (memory) printf("#   physical memory: %s\n", mem_hr);
    printf("#   target/sample  : %.0f ms\n", opt.target_ns / 1e6);
    printf("#   paired trials  : %d\n", opt.trials);
    printf("#   max pages      : %zu\n", opt.max_pages);
    printf("#   walk pages     : %zu\n", opt.walk_pages);
    printf("#   max walk span  : %s\n", span_hr);
#ifdef __APPLE__
    printf("#   placement      : USER_INTERACTIVE QoS + affinity-group hint; not pinned\n");
#else
    printf("#   placement      : %s\n", opt.cpu >= 0 ? "pinned with sched_setaffinity" : "not pinned");
#endif

    if (opt.mode == MODE_GEOMETRY) {
        print_geometry(page_size);
    } else if (opt.mode == MODE_CAPACITY) {
        run_capacity(&opt, page_size, line_size);
    } else if (opt.mode == MODE_WALK) {
        run_walk(&opt, page_size, line_size);
    } else {
        run_capacity(&opt, page_size, line_size);
        run_walk(&opt, page_size, line_size);
    }

    (void)g_sink;
    return EXIT_SUCCESS;
}
