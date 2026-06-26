Day1 Derivations:
Macbook Pro, 14 total cores (10 performance and 4 efficiency), 128 bit SIMD, M4 max clock speed 4.5 GHZ, 4 lanes -> 1.44 TFLOPS/s. For matmul, one entry requires n multiplications and n-1 addition which is 2n-1 floating point operations. Do this n by n times and FLOPS = n^2(2n-1) = 2n^3 - n^2.

Upper bound for AI: maximum # of bytes crossed. Then all bytes should be crossing from DRAM and cache is empty and for 3, NxN matrics, 3N^2 input -> 12N^2 bytes so min AI_flop = n/6

Lower bound for AI: min # of bytes crossed. All bytes in cache. Just for loop 3 times -> 8n^3 -> max AI_flop = 1/4.

DRAM ridge = 1.44TB/400GB =3.6 Flop/Byte

L2 Ridge = 

Predictions (naive matmul):
For n = 2048 square matrix, there's 48 mb of data compared to 16mb L2 Cache so it has to be stored in DRAM. There's the reuse misses: working set exceeds L2, so revisited lines have been evicted and re-pay the DRAM trip. from DRAM to register so we're DRAM bandwidth bound. Then, percent of max flops is AI * 1/DRAM = 1/4 * 1/3.6 =0.0694 ~ 6% of compute capacity. Check ASM if < 2%, real bands are 3-7%. 

Day 2 Derivations:
miss penalty derivation: for a single memory level, AMAT = hit_time + miss_p*miss_r. For multiple caches, AMAT = hit_time1+missr1(hit_time2+missr2(hit_time3+missr3.....))).
L2 is 16MiB so expect the ridge when foot traffic ~ 16MiB.
Measured 4 cliffs: 256KiB (5.96), 16MiB (20.11), 32MiB(33.95), 64MiB(73.63). No plateau from L2 to DRAM instead a steadily increase. Tried running TLB search to isolate the plateau's but TLB AMAT function very similar to cache. (line stride vs page stride (128 B vs 16 KiB)). Clear the L2 -> Dram transition is smeared on MacBook most likely due to some sharing effect. 

Day 3 derivations:
blocked AI = B/4 = 128 FLOP/byte at B=512. bytes per triple tile = 3*(512)^2 * (N/B)^3 so for N = 2048, 128 FLOP/byte. Note that predicted Naive matmul = 1/4*DRAM =1/4 * 100 = 25 Flops/S but measured was 34.8, most likely due to L1 cache reuse. 1/4 was derived under no reuse at all. for blocked matmul, predict 128 Flop/Byte * L2 = 12800 but min(peak, AI*bandwidth) = 1.44 TFLOPS/s. Only using 1 core and got 28.9 GFLOPS/s for blocked matmul because only using 1 core and tile overhead costs a lot. Loops did vectorize. 

