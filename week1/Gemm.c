// gemm_8x8.c — 8x8 register-blocked single-precision GEMM for Apple Silicon (NEON)
//
//   C += A * B,  all row-major, all n x n.   FLOPs = 2*n^3.
//
// Structure:
//   - micro_8x8: 16 accumulator vectors held live across the k-reduction.
//                Only A/B panel loads in the steady state; C touched once.
//   - pack_A / pack_B: copy blocks into contiguous panels so the kernel
//                      sees unit stride (this is what makes 0.25 loads/FMA real).
//   - sgemm: cache-blocked driver (KC, MC) wrapping the microkernel.
//
// Assumes n, MC, KC are multiples of 8 (true for the usual benchmark sizes).
// No edge-tile remainder handling — kept out so the kernel stays readable.
//
// Build (on the M4):   cc -O3 -o gemm gemm_8x8.c
// Inspect:             objdump -d gemm | sed -n '/micro_8x8/,/ret/p' | grep -E 'fmla|ldr|str'
// Run:                 ./gemm 1024

#include <arm_neon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifndef __ARM_NEON
#error "This file requires NEON (build on Apple Silicon / ARM)."
#endif

#define MR 8
#define NR 8
#define MC 256   // A block rows kept resident in L2
#define KC 256   // reduction depth per panel; KC*NR panel ~8KB stays in L1

// ---------------------------------------------------------------------------
// Microkernel: computes an 8x8 block of C += (8 x kc) * (kc x 8).
// pa: packed A panel, layout [k][8 rows]   -> pa[k*8 + i]
// pb: packed B panel, layout [k][8 cols]   -> pb[k*8 + j]
// C : top-left of the 8x8 output tile, leading dim ldc.
// ---------------------------------------------------------------------------
static inline void micro_8x8(int kc, const float *pa, const float *pb,
                             float *C, int ldc) {
    float32x4_t c0a = vdupq_n_f32(0.f), c0b = vdupq_n_f32(0.f);
    float32x4_t c1a = vdupq_n_f32(0.f), c1b = vdupq_n_f32(0.f);
    float32x4_t c2a = vdupq_n_f32(0.f), c2b = vdupq_n_f32(0.f);
    float32x4_t c3a = vdupq_n_f32(0.f), c3b = vdupq_n_f32(0.f);
    float32x4_t c4a = vdupq_n_f32(0.f), c4b = vdupq_n_f32(0.f);
    float32x4_t c5a = vdupq_n_f32(0.f), c5b = vdupq_n_f32(0.f);
    float32x4_t c6a = vdupq_n_f32(0.f), c6b = vdupq_n_f32(0.f);
    float32x4_t c7a = vdupq_n_f32(0.f), c7b = vdupq_n_f32(0.f);

    for (int k = 0; k < kc; k++) {
        // 2 vectors of B (cols 0..3, 4..7), 2 vectors of A (rows 0..3, 4..7).
        // 4 loads total feeding 16 FMAs -> 0.25 loads/FMA.
        float32x4_t b0 = vld1q_f32(pb + 0);
        float32x4_t b1 = vld1q_f32(pb + 4);
        float32x4_t a0 = vld1q_f32(pa + 0);  // rows 0..3 at this k
        float32x4_t a1 = vld1q_f32(pa + 4);  // rows 4..7 at this k

        c0a = vfmaq_laneq_f32(c0a, b0, a0, 0); c0b = vfmaq_laneq_f32(c0b, b1, a0, 0);
        c1a = vfmaq_laneq_f32(c1a, b0, a0, 1); c1b = vfmaq_laneq_f32(c1b, b1, a0, 1);
        c2a = vfmaq_laneq_f32(c2a, b0, a0, 2); c2b = vfmaq_laneq_f32(c2b, b1, a0, 2);
        c3a = vfmaq_laneq_f32(c3a, b0, a0, 3); c3b = vfmaq_laneq_f32(c3b, b1, a0, 3);
        c4a = vfmaq_laneq_f32(c4a, b0, a1, 0); c4b = vfmaq_laneq_f32(c4b, b1, a1, 0);
        c5a = vfmaq_laneq_f32(c5a, b0, a1, 1); c5b = vfmaq_laneq_f32(c5b, b1, a1, 1);
        c6a = vfmaq_laneq_f32(c6a, b0, a1, 2); c6b = vfmaq_laneq_f32(c6b, b1, a1, 2);
        c7a = vfmaq_laneq_f32(c7a, b0, a1, 3); c7b = vfmaq_laneq_f32(c7b, b1, a1, 3);

        pa += MR;
        pb += NR;
    }

    // C += tile, exactly once, outside the k-loop.
#define STORE_ROW(r, cva, cvb)                                              \
    vst1q_f32(&C[(r)*ldc + 0], vaddq_f32(vld1q_f32(&C[(r)*ldc + 0]), cva)); \
    vst1q_f32(&C[(r)*ldc + 4], vaddq_f32(vld1q_f32(&C[(r)*ldc + 4]), cvb))
    STORE_ROW(0, c0a, c0b);
    STORE_ROW(1, c1a, c1b);
    STORE_ROW(2, c2a, c2b);
    STORE_ROW(3, c3a, c3b);
    STORE_ROW(4, c4a, c4b);
    STORE_ROW(5, c5a, c5b);
    STORE_ROW(6, c6a, c6b);
    STORE_ROW(7, c7a, c7b);
#undef STORE_ROW
}

// ---------------------------------------------------------------------------
// Packing. Each packs into MR/NR-wide panels stored [k][lane] contiguous.
// ---------------------------------------------------------------------------
// A block is mc x kc, row-major, leading dim lda. Panel p (rows p*MR..) lives
// at pa + p*(kc*MR); within it pa[k*MR + i] = A[(p*MR+i)*lda + k].
static void pack_A(int mc, int kc, const float *A, int lda, float *pa) {
    for (int ir = 0; ir < mc; ir += MR) {
        float *dst = pa + (ir / MR) * (kc * MR);
        for (int k = 0; k < kc; k++)
            for (int i = 0; i < MR; i++)
                dst[k * MR + i] = A[(ir + i) * lda + k];
    }
}

// B block is kc x nc, row-major, leading dim ldb. Panel p (cols p*NR..) lives
// at pb + p*(kc*NR); within it pb[k*NR + j] = B[k*ldb + (p*NR+j)].
static void pack_B(int kc, int nc, const float *B, int ldb, float *pb) {
    for (int jr = 0; jr < nc; jr += NR) {
        float *dst = pb + (jr / NR) * (kc * NR);
        for (int k = 0; k < kc; k++)
            for (int j = 0; j < NR; j++)
                dst[k * NR + j] = B[k * ldb + jr + j];
    }
}

// ---------------------------------------------------------------------------
// Blocked driver. NC == n (pack full B width per kc-panel).
// C must be zero-initialized by the caller; we accumulate across kc-panels.
// ---------------------------------------------------------------------------
static void sgemm(int n, const float *A, const float *B, float *C,
                  float *packedA, float *packedB) {
    for (int pc = 0; pc < n; pc += KC) {
        int kc = (n - pc < KC) ? (n - pc) : KC;
        pack_B(kc, n, &B[pc * n], n, packedB);
        for (int ic = 0; ic < n; ic += MC) {
            int mc = (n - ic < MC) ? (n - ic) : MC;
            pack_A(mc, kc, &A[ic * n + pc], n, packedA);
            for (int jr = 0; jr < n; jr += NR) {
                const float *pb = packedB + (jr / NR) * (kc * NR);
                for (int ir = 0; ir < mc; ir += MR) {
                    const float *pa = packedA + (ir / MR) * (kc * MR);
                    micro_8x8(kc, pa, pb, &C[(ic + ir) * n + jr], n);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Reference + harness.
// ---------------------------------------------------------------------------
static void naive(int n, const float *A, const float *B, float *C) {
    for (int i = 0; i < n; i++)
        for (int k = 0; k < n; k++) {
            float a = A[i * n + k];
            for (int j = 0; j < n; j++)
                C[i * n + j] += a * B[k * n + j];
        }
}

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    int n = (argc > 1) ? atoi(argv[1]) : 1024;
    if (n % 8) { fprintf(stderr, "n must be a multiple of 8\n"); return 1; }

    float *A = malloc((size_t)n * n * sizeof(float));
    float *B = malloc((size_t)n * n * sizeof(float));
    float *C = malloc((size_t)n * n * sizeof(float));
    float *Cref = malloc((size_t)n * n * sizeof(float));
    float *packedA = malloc((size_t)MC * KC * sizeof(float));
    float *packedB = malloc((size_t)KC * n * sizeof(float));
    if (!A || !B || !C || !Cref || !packedA || !packedB) {
        fprintf(stderr, "alloc failed\n"); return 1;
    }

    srand(1);
    for (size_t i = 0; i < (size_t)n * n; i++) {
        A[i] = (float)rand() / RAND_MAX - 0.5f;
        B[i] = (float)rand() / RAND_MAX - 0.5f;
    }

    // ---- correctness (smaller n to keep the O(n^3) reference cheap) ----
    {
        int m = (n < 256) ? n : 1024;
        memset(C, 0, (size_t)m * m * sizeof(float));
        memset(Cref, 0, (size_t)m * m * sizeof(float));
        // reference and test on the leading m x m submatrix (lda = m here)
        float *a = malloc((size_t)m * m * sizeof(float));
        float *b = malloc((size_t)m * m * sizeof(float));
        for (int i = 0; i < m; i++)
            for (int j = 0; j < m; j++) {
                a[i * m + j] = A[i * n + j];
                b[i * m + j] = B[i * n + j];
            }
        naive(m, a, b, Cref);
        sgemm(m, a, b, C, packedA, packedB);
        double maxrel = 0.0;
        for (int i = 0; i < m * m; i++) {
            double d = fabs((double)C[i] - Cref[i]);
            double r = d / (fabs((double)Cref[i]) + 1e-30);
            if (r > maxrel) maxrel = r;
        }
        printf("correctness (n=%d): max rel err = %.2e  %s\n",
               m, maxrel, maxrel < 1e-3 ? "PASS" : "FAIL");
        free(a); free(b);
    }

    // ---- timing ----
    double peak1 = 144.0;        // your measured single-core peak (GFLOP/s)
    double flops = 2.0 * n * n * (double)n;
    double best = 1e30;
    int reps = 10;
    for (int r = 0; r < reps; r++) {
        memset(C, 0, (size_t)n * n * sizeof(float));
        double t0 = now_s();
        sgemm(n, A, B, C, packedA, packedB);
        double dt = now_s() - t0;
        if (dt < best) best = dt;
    }
    double gflops = flops / best / 1e9;
    printf("n=%d  best=%.4f s  %.1f GFLOP/s  = %.1f%% of single-core peak\n",
           n, best, gflops, 100.0 * gflops / peak1);

    free(A); free(B); free(C); free(Cref); free(packedA); free(packedB);
    return 0;
}