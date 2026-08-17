6 predictions before output:
1. Calibration slope ie ns per unit g where g is the number of times we run the dependent FMA in the for loop. From week 1, our latency was L (latency of one cycle) * t_cyc = (4 ~ 4.5 cyc) * 0.22 \in 0.88 -> 0.99 with 95% confidence. I would expect the slope to be constant ie a horizontal line because there is no g dependence. No accumulatrs 0> p/t_cyc = 1/0.94 =1.0638 GFMA/s which is 17 times worse than matmul. Note that 0.94 comes from latency per one FMA since each FMA is dependent on the previous one unlike the matmul where we can just use 0.22 with registers. 

Actual: 0.9 for large g, 1.13 -> 0.9 as a function of g \in [10, 10000]. Not horizontal as expected, deviation probably comes from latency per loop being slightly non constant depending on overhead. How can we measure this? Time latency from first and last loop in calibration for g = 10000.  

2. Sweep A plateau height (ns/task at saturation): 
First, allocate memory to write results on. Then, wake up scheduler and job pool/queue and submit jobs to the pool. Each worker picks up a job, locks, pops, returns. Queue head/tail move in L2. So we have cost of malloc + producer lock -> signal -> unlock + worker lock -> pop -> unlock + free node. Don't really know how to quantify this without running the code.

Actual:
There is no plateau and ns/task continually increases as the number of workers increase. That means there is some worker contention occurring that is slowing down the tasks. We have the dispatch cost for no contention as 33ns/task for g = 10 -> 76.2 ns/task for g = 100. Compared to calibration which had g= 10 -> 100, ns/task -> 10*ns/task. We know overhead difference is roughly 22 ns for g = 10. But now for g = 100, the overhead is negative(?). So with no queue, the core goes FMA_1 -> wait until completion -> FMA_2 -> ... but with a queue the core goes FMA_1^{k} -> FMA_1^{k+1}  -> .. -> FMA_2^{k} -> ...

3. Is plateau determined by consumer or producer?
Consumer determines size of queue and task time. Producer determines number of workers and scheduling policy. Assume optimal scheduling policy, then for sweep A time to signal/lock/unlock for producer is much larger than task time and size of queue. Bottlenecked by producer, hard to quantify without knowing scheduling OS. 

Actual: Plateau shows up at roughly W = 10 and holds steady. For sweep A, the plateau maybe occurs because the cost is per task not per worker so since we max out on cores we can't really become more efficient. Note that when asked by Fable, it produced an incorrect answer. To be honest, I don't know why it happens.

4. Sweep A shape past plateau
N/A

5. Sweep B efficiency at W = 10
N/A

6. The bend at W = 11
N/A