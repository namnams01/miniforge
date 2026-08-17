Day 1:
What does a multi level page walk cost component by component?

Day2:

Day 3:
Working through the fixed block pool. The idea is to start with a contiguous portion of memory and subdivide into n blocks that can be assigned via request. The issue is the amount of checks you have to write to make sure everything is valid and establishing an API contract between the user and our service. Wrote all the tests specified in the MiniForge document and ASan validation and what not, everything works. A lot of the lessons were C syntax; for instance creating a linked list of nodes using FreeNode that wrote the header bytes into calloc bins by hand. Wrote a suite of test cases and what not and everything passed. Passed ASAN test.

Day 4:
Need to finish coalescing/segmentation extension of block pool. Should probably circle back to Day 2 and try to to do that earnestly. Kind of regurgitating a little here but 1 machine word = 8 bytes, structs on a 64 bit machine want 16 byte alignment so one bool and one pointer is 9 bytes with 7 bytes of padding. Builing segregated pool with coalescing. Not super transferrable to KV cache for week10 but very important it seems for everything memory request related that isn't a paged allocator. The big issue is writing tests; deferring to Sol 5.6 for these. So much code but definitely starting to get the hang of C syntax and what not, very different than Python. Little difficulty in trying to find out best way to split a block: need to add new block to pool, change header + footer, change size_block_index of both blocks, etc. Split/coalescing works, more tests can be written about topologies of coalescing and multiple cycles and safe pointer validation but low roi right now. "Which class breaks first" only applies to the segregated pool. It depends strongly on the request burden, a set of requests of wildly varying sizes especially with a large top request near 4096 is going to break almost immediately if we don't do FIFO but instead serve requests from largest size -> smallest size. I would wager the biggest class breaks first on average simply because we're bottlenecked on memory; in the random ordering there will be a lot of fragmentation from sequences like large request -> small request -> large request. 

Day 5:
Cold Drill: there exists a fragmentation failure iff number of free bytes > requested size and max free block size < requested size. Thus, for a random load, I would expect the largest class size to break first because the smaller sizes will fragment in different locations so we cannot coalesce them. Prediction: Throughput ordering between bump allocator, malloc, and segregated pool. Throughput is number of operations per elapsed time. I would expect bump allocator to be the fastest because it's just pointer addition. The original wording of the primary question incorrectly combined uniform KV blocks with a multi-size-class hypothesis. Before observing results, the experiment was split into (A) segregated-pool fragmentation under a registered varied-size trace and (B) allocator comparison under a uniform KV-block trace. 
Experiment A — segregated-pool fragmentation

Arena size: 4*4096 = 16384 B
Existing size-class boundaries: powers of 2
Number of operations per trace: 1000
Request-size distribution: P(n) = 1/(2.18(n+1)),randomly drawn from [2^(n-1),2^n]
Allocation/free ordering or object-lifetime rule: For operation index i starting at 1:
- If i mod 4 == 0 and at least one allocation is live:
    free the oldest live allocation (FIFO).
- Otherwise:
    allocate a newly sampled request.

“Breaks first” criterion:
First request class for which allocation fails while
total_free >= aligned_request
and largest_free_extent < aligned_request.

Categorical prediction:
Largest request class breaks first.

Directional curve prediction:
Internal fragmentation: Surely increases as a function of operation index, not monotonically, but on average.
External fragmentation: I think external fragmentation increases and then eventually plateuas with some variance.  