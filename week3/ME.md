# Allocator profiling harness

This package profiles the three submitted C allocators and keeps the repaired
Thursday protocol as two distinct experiments.

## Run it

Requirements:

- a C11 compiler (`clang` or `gcc`);
- Python 3;
- `numpy` and `matplotlib`.

On macOS or Linux:

```sh
./run.sh
```

That command:

1. compiles the harness with warnings enabled and optimization on;
2. runs the complete submitted allocator test suite plus a bump-allocator
   alignment/exhaustion test;
3. executes both experiments;
4. writes raw CSVs, three PNG plots, and `results/report.md`.

Run the ASan/UBSan validation separately:

```sh
make sanitize

# Optional on hosts where LeakSanitizer is supported:
make sanitize ASAN_OPTIONS=detect_leaks=1
```

## Experiment A: varied-size segregated pool

Frozen default protocol:

- arena: 16,384 bytes (`2^14`);
- 30 deterministic traces, seeds 1 through 30;
- 1,000 operations per trace;
- every fourth operation frees the oldest logical allocation (FIFO);
- otherwise allocate a new object;
- select request class `n` from 1 through 12 with
  `P(n) = 1 / [(H_13 - 1)(n + 1)]`;
- within class `n`, sample uniformly from integer sizes
  `[2^(n-1) + 1, 2^n]`;
- registered categorical prediction: request class 12 is the modal class
  producing the first fragmentation-induced failure.

An allocation failure is classified using the allocator's **required block
extent**, including header/footer and alignment:

```text
capacity:       total_free_extent < required_block_size
fragmentation:  total_free_extent >= required_block_size
                and largest_free_extent < required_block_size
bug/policy:     largest_free_extent >= required_block_size but alloc failed
```

The plotted metrics are deliberately separate:

```text
internal waste = allocated payload capacity - live requested bytes
external score = 1 - largest free extent / total free extent
utilization    = 1 - total free extent / arena size
```

The CSV reports both the sampled request class (`1..12`) and the allocator's
free-list class (`0..7`). This matters because allocator classes operate on
block extents after metadata and alignment, not raw request sizes.

## Experiment B: uniform KV-style blocks

The identical uniform trace is replayed against:

- the submitted fixed-block pool;
- system `malloc`/`free`;
- the submitted bump allocator.

Defaults are a 256-byte request and a 256 KiB local arena. The 256-byte value
matches the submitted fixed-pool test configuration; it is **not** claimed to
be a model-independent KV-cache byte size. The larger arena permits all 1,000
operations to form a stable timing sample while the retained-byte curve still
shows the bump allocator's inability to reclaim individual frees.

Timing uses the common successful prefix, rotates allocator order across 1,000
repetitions, and reports the median and interquartile range. Trace generation,
allocator initialization, cleanup, CSV writing, and plotting are outside the
timed interval. The working directional prediction is
`bump > fixed_pool > malloc`; unlike the class-12 call, it is not treated as a
separate formally registered categorical hypothesis.

Interpretation limits:

- bump `free` is intentionally a no-op; its abandoned allocations are reported
  as unreclaimed dead bytes, not external fragmentation;
- fixed-pool blocks are interchangeable, so ordinary external fragmentation is
  absent on the uniform workload;
- portable code cannot inspect private `malloc` free extents, so the harness
  reports its usable-size padding but makes no external-fragmentation claim;
- `malloc` uses the system heap while fixed and bump use configured arenas;
- throughput is machine/runtime specific. Re-run on the target Mac or Linux
  host before using the numbers in a final lab note.

## Configuration

The executable exposes the main protocol parameters:

```sh
build/allocator_profile --help

# Example: a model-derived uniform block size
build/allocator_profile --all \
  --kv-block-size 4096 \
  --kv-arena-size 4194304 \
  --output-dir results
python3 scripts/plot_results.py results
```

Do not tune Experiment A parameters after seeing its result if the goal is to
grade the registered hypothesis. Parameter changes define a new experiment.

## Source layout

```text
vendor/   exact submitted allocator files
src/      integrated C profiler and trace generator
scripts/  plots and report generator
results/  generated CSVs, plots, metadata, and report
```

The profiler includes the submitted `.c` files into one translation unit. That
is intentional: the segregated allocator's implementation and metadata helpers
are `static`, and direct inclusion instruments them without changing its public
shape or allocation behavior.
