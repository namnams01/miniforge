// microkernel_bench.c
// Step-1 diagnostic: is the 8x8 NEON microkernel latency- or bandwidth-bound?
// Build (M4 Max):  clang -O3 -march=native -o mk microkernel_bench.c
// Run:             ./mk            (defaults K=256, iters=200000)
//                  ./mk 384 300000 (custom K, iters)
//
// Read the "%% of peak" line. ~55-70%% => latency-bound, the register tile
// was the whole game. ~25-30%% => something's wrong (layout/cache/store), stop
// and diagnose before building the blocking nest.

#include <arm_neon.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// Single-thread SP peak for one M4 Max P-core. ESTIMATE — verify against your
// own clock. peak = clock(GHz) * 2(FMA) * 4(NEON fp32 lanes) * 4(FP pipes).
// ~4.5 GHz -> ~144. Adjust if your sustained clock differs.
#define PEAK_GFLOPS 144.0

// C[8x8] += A_panel[8 x K] * B_panel[K x 8]
// Ap packed: Ap[k*8 + i] = A[i][k]   (8 contiguous rows per k-step)
// Bp packed: Bp[k*8 + j] = B[k][j]   (8 contiguous cols per k-step)
// C column-major, leading dim ldc, accumulated into.
static void microkernel_8x8(int K, const float* Ap, const float* Bp,
                            float* C, int ldc) {
    float32x4_t c0l=vdupq_n_f32(0), c0h=vdupq_n_f32(0);
    float32x4_t c1l=vdupq_n_f32(0), c1h=vdupq_n_f32(0);
    float32x4_t c2l=vdupq_n_f32(0), c2h=vdupq_n_f32(0);
    float32x4_t c3l=vdupq_n_f32(0), c3h=vdupq_n_f32(0);
    float32x4_t c4l=vdupq_n_f32(0), c4h=vdupq_n_f32(0);
    float32x4_t c5l=vdupq_n_f32(0), c5h=vdupq_n_f32(0);
    float32x4_t c6l=vdupq_n_f32(0), c6h=vdupq_n_f32(0);
    float32x4_t c7l=vdupq_n_f32(0), c7h=vdupq_n_f32(0);

    for (int k = 0; k < K; ++k) {
        float32x4_t aL = vld1q_f32(Ap);
        float32x4_t aH = vld1q_f32(Ap + 4);
        float32x4_t b0 = vld1q_f32(Bp);
        float32x4_t b1 = vld1q_f32(Bp + 4);
        Ap += 8; Bp += 8;

        c0l = vfmaq_laneq_f32(c0l, aL, b0, 0); c0h = vfmaq_laneq_f32(c0h, aH, b0, 0);
        c1l = vfmaq_laneq_f32(c1l, aL, b0, 1); c1h = vfmaq_laneq_f32(c1h, aH, b0, 1);
        c2l = vfmaq_laneq_f32(c2l, aL, b0, 2); c2h = vfmaq_laneq_f32(c2h, aH, b0, 2);
        c3l = vfmaq_laneq_f32(c3l, aL, b0, 3); c3h = vfmaq_laneq_f32(c3h, aH, b0, 3);
        c4l = vfmaq_laneq_f32(c4l, aL, b1, 0); c4h = vfmaq_laneq_f32(c4h, aH, b1, 0);
        c5l = vfmaq_laneq_f32(c5l, aL, b1, 1); c5h = vfmaq_laneq_f32(c5h, aH, b1, 1);
        c6l = vfmaq_laneq_f32(c6l, aL, b1, 2); c6h = vfmaq_laneq_f32(c6h, aH, b1, 2);
        c7l = vfmaq_laneq_f32(c7l, aL, b1, 3); c7h = vfmaq_laneq_f32(c7h, aH, b1, 3);
    }

    #define ADD_COL(j, lo, hi) \
        vst1q_f32(C+(j)*ldc+0, vaddq_f32(vld1q_f32(C+(j)*ldc+0), lo)); \
        vst1q_f32(C+(j)*ldc+4, vaddq_f32(vld1q_f32(C+(j)*ldc+4), hi));
    ADD_COL(0,c0l,c0h); ADD_COL(1,c1l,c1h); ADD_COL(2,c2l,c2h); ADD_COL(3,c3l,c3h);
    ADD_COL(4,c4l,c4h); ADD_COL(5,c5l,c5h); ADD_COL(6,c6l,c6h); ADD_COL(7,c7l,c7h);
    #undef ADD_COL
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// 64-byte aligned alloc so vld1q never straddles a line boundary by accident.
static float* alloc_f32(size_t n) {
    void* p = NULL;
    if (posix_memalign(&p, 64, n * sizeof(float)) != 0) { perror("alloc"); exit(1); }
    return (float*)p;
}

int main(int argc, char** argv) {
    int K     = (argc > 1) ? atoi(argv[1]) : 256;     // K=256 -> A,B,C all in L1
    long iters = (argc > 2) ? atol(argv[2]) : 200000;

    const int ldc = 8;
    float* Ap = alloc_f32((size_t)8 * K);
    float* Bp = alloc_f32((size_t)8 * K);
    float* C  = alloc_f32((size_t)8 * ldc);

    // Deterministic fill in [0,1) so the reference check is reproducible.
    srand(1);
    for (long i = 0; i < 8L * K; ++i) Ap[i] = (float)rand() / RAND_MAX;
    for (long i = 0; i < 8L * K; ++i) Bp[i] = (float)rand() / RAND_MAX;

    // ---- correctness: one clean kernel run vs naive reference ----
    for (int i = 0; i < 64; ++i) C[i] = 0.0f;
    microkernel_8x8(K, Ap, Bp, C, ldc);

    double max_abs_err = 0.0;
    for (int i = 0; i < 8; ++i) {           // row
        for (int j = 0; j < 8; ++j) {       // col
            double ref = 0.0;
            for (int k = 0; k < K; ++k)
                ref += (double)Ap[k*8 + i] * (double)Bp[k*8 + j];
            double got = C[j*ldc + i];      // column-major
            double e = fabs(got - ref);
            if (e > max_abs_err) max_abs_err = e;
        }
    }
    double tol = 1e-3 * K;  // fp32 accumulation slack, scales with K
    printf("correctness: max_abs_err = %.3e  (tol %.3e)  -> %s\n",
           max_abs_err, tol, max_abs_err < tol ? "PASS" : "FAIL");
    if (max_abs_err >= tol) {
        fprintf(stderr, "kernel is wrong; benchmark would be meaningless. stopping.\n");
        return 1;
    }

    // ---- warmup ----
    for (int i = 0; i < 64; ++i) C[i] = 0.0f;
    for (long it = 0; it < 2000; ++it) microkernel_8x8(K, Ap, Bp, C, ldc);

    // ---- timed loop. C accumulates across iters; FLOP count is unaffected. ----
    for (int i = 0; i < 64; ++i) C[i] = 0.0f;
    double t0 = now_sec();
    for (long it = 0; it < iters; ++it) microkernel_8x8(K, Ap, Bp, C, ldc);
    double t1 = now_sec();

    // sink so the compiler can't elide the loop
    volatile float sink = 0.0f;
    for (int i = 0; i < 64; ++i) sink += C[i];
    (void)sink;

    double secs   = t1 - t0;
    double flops  = 2.0 * 8.0 * 8.0 * (double)K * (double)iters; // 2*M*N*K
    double gflops = flops / secs / 1e9;

    printf("K=%d  iters=%ld  time=%.4f s\n", K, iters, secs);
    printf("GFLOP/s = %.2f\n", gflops);
    printf("%% of peak (%.0f) = %.1f%%\n", PEAK_GFLOPS, 100.0 * gflops / PEAK_GFLOPS);

    free(Ap); free(Bp); free(C);
    return 0;
}