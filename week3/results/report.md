# Allocator profiling report

Generated on `Linux-6.18.35-x86_64-with-glibc2.39` with Python `3.12.13`.

## Executive verdict

- Largest-class hypothesis: **FALSIFIED**. Class 12 was the registered prediction; the modal first-failing class was 10 across 24 qualifying traces (6 had no fragmentation-induced failure). The modal count was 5; class 12 occurred 4 times.
- Throughput-order prediction: **SUPPORTED**. Measured median order: bump > fixed_pool > malloc.
- Correctness during profiling: **PASS**. Observed 0 allocator-policy/bug failures.

## Experiment A — varied-size segregated pool

Protocol: 30 seeds × 1000 operations, 16.00 KiB arena. Every fourth operation frees the oldest logical allocation.

The run recorded 53 fragmentation failures and 9451 ordinary capacity failures. The categorical result uses only the first fragmentation failure in each seeded trace.
The modal sampled request class was 10; after allocator metadata and alignment, the modal free-list class index was 1, 4. The second view is reported because raw request classes and allocator block classes do not map one-to-one.
The categorical grade follows the predeclared modal-first aggregation rule. The counts are dispersed, so this falsifies the exact registered call on this protocol but is not strong evidence that the observed modal class is universally privileged.

| Curve checkpoint | 10% of trace | 50% of trace | 90% of trace |
|---|---:|---:|---:|
| Median internal waste | 556 B | 1.96 KiB | 2.80 KiB |
| Median external score | 0.043 | 0.000 | 0.000 |

External fragmentation is `1 - largest_free_extent / total_free_extent`. A failure is labeled fragmentation only when aggregate free extent is large enough but the largest contiguous extent is too small for the required block size.

## Experiment B — uniform-block comparison

Protocol: 1000 operations at 256 bytes per request; fixed and bump arena size 256.00 KiB. The malloc baseline uses the system heap.

| Allocator | Median Mops/s | IQR Mops/s | First failure | Successful allocs | Peak retained | Final internal waste | Final unreclaimed dead |
|---|---:|---:|---:|---:|---:|---:|---:|
| fixed_pool | 169.520 | 165.590–174.856 | none | 750 | 125.25 KiB | 0 B | 0 B |
| malloc | 122.070 | 121.773–122.369 | none | 750 | 129.16 KiB | 3.91 KiB | 0 B |
| bump | 501.756 | 494.315–504.286 | none | 750 | 187.50 KiB | 0 B | 62.50 KiB |

## Build-vs-buy interpretation

For a uniform KV-style block contract with arbitrary individual frees, the fixed pool is the structurally matched custom allocator: it reuses any returned block and has no external-fragmentation mechanism. The bump allocator's timing is meaningful, but its individual `free` is a no-op; its growing dead-byte curve is the cost. `malloc` remains the production general-purpose baseline and should not be judged on portable external fragmentation here because the harness cannot inspect its private heap extents.

This licenses a workload-specific component verdict, not general allocator superiority.

## Lab-log delta

- Registered categorical call: class 12 first. Result: falsified (modal class 10).
- Internal-waste direction: median checkpoints were 556 B, 1.96 KiB, and 2.80 KiB.
- External-curve checkpoints were 0.043, 0.000, and 0.000; inspect the plotted IQR before assigning a smooth shape.
- Directional throughput call: bump > fixed_pool > malloc. Result: supported (bump > fixed_pool > malloc).

## Files

- `experiment_a_operations.csv`: one row per operation per seed.
- `experiment_a_seed_summary.csv`: first qualifying failure and failure counts per seed.
- `experiment_b_operations.csv`: memory behavior for each allocator.
- `experiment_b_benchmark.csv`: every timing repetition.
- `experiment_b_summary.csv`: behavior summary.
- PNG files: publication-ready plots generated from those CSVs.
