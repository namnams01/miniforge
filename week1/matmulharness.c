#include <stdio.h>
#include <stdlib.h>
#include <time.h>   // add this include at the top
#include <string.h>
#include <arm_neon.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

typedef void (*matmul_fn)(int n, const float *A, const float *B, float *C);

void naive(int n, const float *A, const float *B, float *C) {
	for (int i = 0; i < n; i++){
		for (int k = 0; k < n; k++){
			for (int j = 0; j< n; j++)
				C[i*n+j] += A[i*n+k] * B[k*n+j];
		
		}

	}
}

void blocked(int n, int E, const float *A, const float *B, float *C) {

for (int ii=0; ii<n; ii+=E)
 for (int kk=0; kk<n; kk+=E)
  for (int jj=0; jj<n; jj+=E)
   for (int i=ii; i<ii+E; i++)
    for (int k=kk; k<kk+E; k++)
     for (int j=jj; j<jj+E; j++)
       C[i*n+j] += A[i*n+k]*B[k*n+j];
}

void vectorized(int n, const float *A, const float *B, float *C){
	for (int i = 0; i < n; i++){
		for (int k = 0; k < n; k++){
			float32x4_t va = vdupq_n_f32(A[i*n+k]);
			for (int j = 0; j< n; j+=4){
				float32x4_t vb = vld1q_f32(&B[k*n+j]);
				float32x4_t vc = vld1q_f32(&C[i*n+j]);
				vc = vfmaq_f32(vc, va, vb);
				vst1q_f32(&C[i*n+j], vc);
			}
		}

	}
}

void L1blocked(int n, int E, const float *A, const float *B, float *C) {

for (int ii=0; ii<n; ii+=E)
 for (int kk=0; kk<n; kk+=E)
  for (int jj=0; jj<n; jj+=E)
   for (int i=ii; i<ii+E; i++)
    for (int k=kk; k<kk+E; k++)
     for (int j=jj; j<jj+E; j++)
       C[i*n+j] += A[i*n+k]*B[k*n+j];
}

void register_blocking(int n, int E, const float *A, const float *B, float *C){


}



int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a;   // the void* cast lesson, live
    double y = *(const double *)b;
    return (x > y) - (x < y);        // -1, 0, or 1
}

int main(void) {
    int n = 2048;                              // declare n FIRST
    int E = 32;
    size_t bytes = (size_t)n * n * sizeof(float);
    float *A = malloc(bytes);
    float *B = malloc(bytes);
    float *C = malloc(bytes);
    if (!A || !B || !C) {printf("error\n"); return 1;}

    for (size_t idx = 0; idx < (size_t)n*n; idx++) {
        A[idx] = (float)(idx % 100);
        B[idx] = (float) (idx % 74);
    }
    int N = 10;
    double *samples = malloc(N * sizeof(double));
    memset(C, 0, bytes);  

    vectorized(n, A, B, C);                      // call the function; C is filled
    for (int w = 0; w < 3; w++) vectorized(n,A, B, C);
    memset(C, 0, bytes);
    for (int i = 0; i < N; i++) {
    	memset(C, 0, bytes);  
        double t0 = now_sec();
	vectorized(n, A, B, C);
	double t1 = now_sec();
	samples[i] = t1 - t0;   // elapsed seconds, as a double
    }

    double sum = 0.0;
    for (size_t idx = 0; idx < (size_t)n*n; idx++)
	sum += C[idx];

    qsort(samples, N, sizeof(double), cmp_double);

    double flops = 2.0 * n * n * n - (double)n * n;   // 2n^3 - n^2
    double median = samples[N/2];
    double gflops = flops / median / 1e9;
    double iqr = samples[3*N/4] - samples[N/4];

    printf("checksum: %f\n", sum);
    printf("n = %d\n", n);
    printf("median time: %f s\n", median);
    printf("IQR: %f s\n", iqr);
    printf("GFLOP/s: %f\n", gflops);
    printf("%% of peak: %f\n", gflops / 1444.0 * 100.0);
    printf("checksum: %f\n", sum);
    free(A); free(B); free(C);
    return 0;
}