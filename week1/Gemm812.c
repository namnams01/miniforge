// gemm_8x12.c — 8x12 register-blocked single-precision GEMM (NEON).
//
// Same structure as gemm_8x8.c, widened to nr=12: 24 accumulator vectors,
// 3 B-vectors + 2 A-vectors live = 29 registers (<= 32). loads/FMA = 1/8+1/12 = 0.21.
//
// nr=12 => n must be divisible by 24 (12 for cols, 8 for rows). Use 1008 or 1152.
//
// Build:    clang -O3 -o gemm12 gemm_8x12.c
// Inspect:  objdump -d gemm12 | sed -n '/micro_8x12/,/ret/p' | grep -E 'fmla|ldr|str'
// Run:      ./gemm12 1008

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
#define NR 12
#define MC 256
#define KC 256

// 8x12 microkernel: C += (8 x kc) * (kc x 12).
// pa: [k][8 rows] -> pa[k*8 + i];  pb: [k][12 cols] -> pb[k*12 + j].
static inline void micro_8x12(int kc, const float *pa, const float *pb,
                              float *C, int ldc) {
    float32x4_t c0a=vdupq_n_f32(0.f), c0b=vdupq_n_f32(0.f), c0c=vdupq_n_f32(0.f);
    float32x4_t c1a=vdupq_n_f32(0.f), c1b=vdupq_n_f32(0.f), c1c=vdupq_n_f32(0.f);
    float32x4_t c2a=vdupq_n_f32(0.f), c2b=vdupq_n_f32(0.f), c2c=vdupq_n_f32(0.f);
    float32x4_t c3a=vdupq_n_f32(0.f), c3b=vdupq_n_f32(0.f), c3c=vdupq_n_f32(0.f);
    float32x4_t c4a=vdupq_n_f32(0.f), c4b=vdupq_n_f32(0.f), c4c=vdupq_n_f32(0.f);
    float32x4_t c5a=vdupq_n_f32(0.f), c5b=vdupq_n_f32(0.f), c5c=vdupq_n_f32(0.f);
    float32x4_t c6a=vdupq_n_f32(0.f), c6b=vdupq_n_f32(0.f), c6c=vdupq_n_f32(0.f);
    float32x4_t c7a=vdupq_n_f32(0.f), c7b=vdupq_n_f32(0.f), c7c=vdupq_n_f32(0.f);

    for (int k = 0; k < kc; k++) {
        float32x4_t b0 = vld1q_f32(pb + 0);   // cols 0..3
        float32x4_t b1 = vld1q_f32(pb + 4);   // cols 4..7
        float32x4_t b2 = vld1q_f32(pb + 8);   // cols 8..11
        float32x4_t a0 = vld1q_f32(pa + 0);   // rows 0..3
        float32x4_t a1 = vld1q_f32(pa + 4);   // rows 4..7

        c0a=vfmaq_laneq_f32(c0a,b0,a0,0); c0b=vfmaq_laneq_f32(c0b,b1,a0,0); c0c=vfmaq_laneq_f32(c0c,b2,a0,0);
        c1a=vfmaq_laneq_f32(c1a,b0,a0,1); c1b=vfmaq_laneq_f32(c1b,b1,a0,1); c1c=vfmaq_laneq_f32(c1c,b2,a0,1);
        c2a=vfmaq_laneq_f32(c2a,b0,a0,2); c2b=vfmaq_laneq_f32(c2b,b1,a0,2); c2c=vfmaq_laneq_f32(c2c,b2,a0,2);
        c3a=vfmaq_laneq_f32(c3a,b0,a0,3); c3b=vfmaq_laneq_f32(c3b,b1,a0,3); c3c=vfmaq_laneq_f32(c3c,b2,a0,3);
        c4a=vfmaq_laneq_f32(c4a,b0,a1,0); c4b=vfmaq_laneq_f32(c4b,b1,a1,0); c4c=vfmaq_laneq_f32(c4c,b2,a1,0);
        c5a=vfmaq_laneq_f32(c5a,b0,a1,1); c5b=vfmaq_laneq_f32(c5b,b1,a1,1); c5c=vfmaq_laneq_f32(c5c,b2,a1,1);
        c6a=vfmaq_laneq_f32(c6a,b0,a1,2); c6b=vfmaq_laneq_f32(c6b,b1,a1,2); c6c=vfmaq_laneq_f32(c6c,b2,a1,2);
        c7a=vfmaq_laneq_f32(c7a,b0,a1,3); c7b=vfmaq_laneq_f32(c7b,b1,a1,3); c7c=vfmaq_laneq_f32(c7c,b2,a1,3);

        pa += MR;
        pb += NR;
    }

#define STORE_ROW(r, cva, cvb, cvc)                                         \
    vst1q_f32(&C[(r)*ldc + 0], vaddq_f32(vld1q_f32(&C[(r)*ldc + 0]), cva)); \
    vst1q_f32(&C[(r)*ldc + 4], vaddq_f32(vld1q_f32(&C[(r)*ldc + 4]), cvb)); \
    vst1q_f32(&C[(r)*ldc + 8], vaddq_f32(vld1q_f32(&C[(r)*ldc + 8]), cvc))
    STORE_ROW(0, c0a, c0b, c0c);
    STORE_ROW(1, c1a, c1b, c1c);
    STORE_ROW(2, c2a, c2b, c2c);
    STORE_ROW(3, c3a, c3b, c3c);
    STORE_ROW(4, c4a, c4b, c4c);
    STORE_ROW(5, c5a, c5b, c5c);
    STORE_ROW(6, c6a, c6b, c6c);
    STORE_ROW(7, c7a, c7b, c7c);
#undef STORE_ROW
}

static void pack_A(int mc, int kc, const float *A, int lda, float *pa) {
    for (int ir = 0; ir < mc; ir += MR) {
        float *dst = pa + (ir / MR) * (kc * MR);
        for (int k = 0; k < kc; k++)
            for (int i = 0; i < MR; i++)
                dst[k * MR + i] = A[(ir + i) * lda + k];
    }
}

static void pack_B(int kc, int nc, const float *B, int ldb, float *pb) {
    for (int jr = 0; jr < nc; jr += NR) {
        float *dst = pb + (jr / NR) * (kc * NR);
        for (int k = 0; k < kc; k++)
            for (int j = 0; j < NR; j++)
                dst[k * NR + j] = B[k * ldb + jr + j];
    }
}

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
                    micro_8x12(kc, pa, pb, &C[(ic + ir) * n + jr], n);
                }
            }
        }
    }
}

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
    int n = (argc > 1) ? atoi(argv[1]) : 1008;
    if (n % 24) { fprintf(stderr, "n must be a multiple of 24 (e.g. 1008, 1152)\n"); return 1; }

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

    {
        int m = (n < 240) ? n : 240;   // divisible by 24
        float *a = malloc((size_t)m * m * sizeof(float));
        float *b = malloc((size_t)m * m * sizeof(float));
        memset(C, 0, (size_t)m * m * sizeof(float));
        memset(Cref, 0, (size_t)m * m * sizeof(float));
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

    double peak1 = 144.0;
    double flops = 2.0 * n * n * (double)n;
    double best = 1e30;
    for (int r = 0; r < 5; r++) {
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