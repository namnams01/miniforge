Day 1:
Context switch, 2 threads A and B: save regs(A) to k-stack(A). saves the current state of A to L1 so from pointer chase in week 1, 5.96 ns. Then, OS entering kernel mode and jumping to trap is ignored for now. Saving regs(A) to prod struct(A) is cheap we'll say 5.96 ns again. Then regs(B) to prod struct(B), switching to k-stack(B), restoring regs(B) is all roughly the same time as above so 5.96*4=23.84 ns total. This is like worst case scenario with all regs filled and not being able save independently. 

Day 2:
The dependent-FMA calibration converged to 0.916 ns/FMA, inside my predicted 0.88–0.99 ns interval. Fine-grained pool throughput degraded sharply as workers increased and plateaued near 190–200k tasks/s at W≥16. The high system time and context-switch count indicate synchronization and scheduler overhead, but the current experiment cannot separate mutex contention, wake behavior, allocator traffic, and instrumentation-induced false sharing. Coarse tasks scaled to 10.16× at 16 workers and then saturated, consistent with exhausting useful CPU capacity. My next experiment will remove the unused packed atomics, use an identical direct-execution baseline, and time completion separately from pool destruction.

Day 3:
We're going to try to find out why dispatch A is so inefficient, we have our leading causes from Day 2. First, we should isolate dispatch A and B and run user/bin to locate where the involuntary context switches are coming from. A dominates the kernel noise taking up 99.9% of it. A dispatched 25.0M measured tasks (10 points × 5 reps × 500k). So: 333.6 s sys / 25M ≈ 13.3 µs of kernel CPU per task — aggregated across cores, which is why it exceeds the 5.2 µs wall plateau; at ~334 s sys over 68 s wall, roughly five cores sat continuously in the kernel. And 30.1M switches / 25M ≈ 1.2 context switches per task. At 23.84 ns from Day 1, we expect 23.84 ns * 30.1 M = 0.7 s which is off by a factor of 470 in total system time. Measure empirically: 

Day 4:
Cold drill: acquire/release on head/tail. We have some ring buffer with N slots, the address of the ith task is i mod N. Suppose the logical address of the head is at H. The consumer can now see that H is ready to be read, the consumer reads it, and release-stores it. The producer can now atomically acquire-load this slot and write a new task description. Now the consumer can go along the ring buffer until it hits T where T is the logical position of the tail. If the tail is visible to the consumer, the consumer can do the same thing as before but it cannot wrap around until the producer release-stores new tasks for logical positions T+i (head - tail < N). So at the head, the producer can load what the consumer wrote. The producer can then store a new task description at the new tail T+1. The consumer releases at the head and acquires at the tail. What's really being acquired/released is permission to read/write/see what's at each slot.




