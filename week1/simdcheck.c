#include <arm_neon.h>
#include <stdio.h>

int main(void) {
    float a[4] = {1, 2, 3, 4};
    float b[4] = {10, 20, 30, 40};
    float c[4];

    float32x4_t va = vld1q_f32(a);      // load 4 floats into a 128-bit vector register
    float32x4_t vb = vld1q_f32(b);      // load 4 more
    float32x4_t vc = vaddq_f32(va, vb); // add all 4 lanes in one instruction
    vst1q_f32(c, vc);                   // store the 4 results back

    printf("%.0f %.0f %.0f %.0f\n", c[0], c[1], c[2], c[3]);  // 11 22 33 44
    return 0;
}