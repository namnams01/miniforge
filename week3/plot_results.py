#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import json
import platform
import statistics
from collections import Counter, defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


COLORS = {
    "fixed_pool": "#246B8E",
    "malloc": "#6C757D",
    "bump": "#D97706",
    "fragmentation": "#B42318",
    "capacity": "#7A5AF8",
    "prediction": "#D97706",
}


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def as_int(row: dict[str, str], key: str) -> int:
    return int(row[key])


def as_float(row: dict[str, str], key: str) -> float:
    return float(row[key])


def quantile_band(values: list[float]) -> tuple[float, float, float]:
    array = np.asarray(values, dtype=float)
    return tuple(float(x) for x in np.quantile(array, [0.25, 0.5, 0.75]))


def set_style() -> None:
    plt.rcParams.update(
        {
            "figure.facecolor": "#FAFAF8",
            "axes.facecolor": "#FAFAF8",
            "axes.edgecolor": "#C8C8C2",
            "axes.labelcolor": "#202020",
            "axes.titleweight": "bold",
            "axes.titlesize": 11,
            "font.size": 9,
            "grid.color": "#D9D9D4",
            "grid.alpha": 0.6,
            "legend.frameon": False,
            "savefig.facecolor": "#FAFAF8",
        }
    )


def plot_experiment_a(results: Path, output: Path) -> dict[str, object]:
    rows = read_csv(results / "experiment_a_operations.csv")
    summaries = read_csv(results / "experiment_a_seed_summary.csv")

    by_operation: dict[int, dict[str, list[float]]] = defaultdict(
        lambda: defaultdict(list)
    )
    fragmentation_events: list[int] = []
    capacity_events: list[int] = []
    for row in rows:
        operation = as_int(row, "operation_index")
        by_operation[operation]["internal_ratio"].append(
            as_float(row, "internal_fragmentation_ratio")
        )
        by_operation[operation]["internal_bytes"].append(
            as_float(row, "internal_waste")
        )
        by_operation[operation]["external"].append(
            as_float(row, "external_fragmentation")
        )
        by_operation[operation]["utilization"].append(
            as_float(row, "utilization")
        )
        if row["failure_type"] == "external_fragmentation":
            fragmentation_events.append(operation)
        elif row["failure_type"] == "capacity":
            capacity_events.append(operation)

    operations = sorted(by_operation)
    series: dict[str, tuple[np.ndarray, np.ndarray, np.ndarray]] = {}
    for metric in ("internal_ratio", "internal_bytes", "external", "utilization"):
        bands = np.asarray(
            [quantile_band(by_operation[operation][metric]) for operation in operations]
        )
        series[metric] = (bands[:, 0], bands[:, 1], bands[:, 2])

    figure, axes = plt.subplots(3, 1, figsize=(11, 9), sharex=True)
    figure.suptitle(
        "Experiment A — segregated-pool fragmentation",
        fontsize=15,
        fontweight="bold",
        x=0.08,
        ha="left",
    )
    figure.text(
        0.08,
        0.945,
        "Median across seeded traces; shaded regions are interquartile ranges.",
        color="#555555",
    )

    labels = [
        ("internal_ratio", "Internal fragmentation", "Waste / payload capacity"),
        ("external", "External fragmentation", r"$1 - L/F$"),
        ("utilization", "Arena utilization", "Allocated extent / arena"),
    ]
    for axis, (metric, title, ylabel) in zip(axes, labels):
        low, median, high = series[metric]
        axis.plot(operations, median, color="#246B8E", linewidth=1.8, label="Median")
        axis.fill_between(operations, low, high, color="#8EC5DA", alpha=0.35, label="IQR")
        axis.set_title(title, loc="left")
        axis.set_ylabel(ylabel)
        axis.set_ylim(-0.02, 1.06)
        axis.grid(axis="y")

    if fragmentation_events:
        axes[2].scatter(
            fragmentation_events,
            [1.025] * len(fragmentation_events),
            marker="|",
            s=60,
            color=COLORS["fragmentation"],
            alpha=0.6,
            label="Fragmentation failure",
        )
    if capacity_events:
        axes[2].scatter(
            capacity_events,
            [0.985] * len(capacity_events),
            marker="|",
            s=50,
            color=COLORS["capacity"],
            alpha=0.35,
            label="Capacity failure",
        )
    axes[0].legend(loc="upper left", ncol=2)
    axes[2].legend(loc="lower right", ncol=2)
    axes[2].set_xlabel("Operation index")
    figure.tight_layout(rect=(0.03, 0.02, 0.99, 0.93))
    figure.savefig(output / "experiment_a_fragmentation.png", dpi=180)
    plt.close(figure)

    counts = Counter(
        as_int(row, "first_request_class")
        for row in summaries
        if as_int(row, "first_fragmentation_op") > 0
    )
    classes = list(range(1, 13))
    values = [counts.get(request_class, 0) for request_class in classes]
    colors = [
        COLORS["prediction"] if request_class == 12 else "#6A9FB5"
        for request_class in classes
    ]
    qualifying = sum(values)
    no_event = len(summaries) - qualifying

    allocator_counts = Counter(
        as_int(row, "first_allocator_class")
        for row in summaries
        if as_int(row, "first_fragmentation_op") > 0
    )
    allocator_classes = list(range(8))
    allocator_values = [allocator_counts.get(index, 0) for index in allocator_classes]

    figure, axes = plt.subplots(1, 2, figsize=(12, 5.1))
    figure.suptitle(
        "First fragmentation-failing class",
        fontsize=15,
        fontweight="bold",
        x=0.06,
        ha="left",
    )
    figure.text(
        0.06,
        0.91,
        f"{qualifying} qualifying traces; {no_event} traces had no fragmentation failure. "
        "Request class 12 is the registered prediction.",
        color="#555555",
    )
    axes[0].bar(classes, values, color=colors, width=0.72)
    axes[0].set_title("Sampled request class", loc="left")
    axes[0].set_xlabel("Request class n")
    axes[0].set_ylabel("Seed count")
    axes[0].set_xticks(classes)
    axes[0].grid(axis="y")

    allocator_colors = [
        COLORS["prediction"] if index == 7 else "#6A9FB5"
        for index in allocator_classes
    ]
    axes[1].bar(allocator_classes, allocator_values, color=allocator_colors, width=0.72)
    axes[1].set_title("Post-metadata allocator class", loc="left")
    axes[1].set_xlabel("Free-list class index")
    axes[1].set_ylabel("Seed count")
    axes[1].set_xticks(allocator_classes)
    axes[1].grid(axis="y")
    figure.tight_layout(rect=(0.02, 0.03, 0.99, 0.86))
    figure.savefig(output / "experiment_a_first_breaking_class.png", dpi=180)
    plt.close(figure)

    median_internal = series["internal_bytes"][1]
    median_external = series["external"][1]
    return {
        "summaries": summaries,
        "counts": counts,
        "allocator_counts": allocator_counts,
        "qualifying": qualifying,
        "no_event": no_event,
        "median_internal": median_internal,
        "median_external": median_external,
        "operations": operations,
    }


def plot_experiment_b(results: Path, output: Path) -> dict[str, object]:
    operation_rows = read_csv(results / "experiment_b_operations.csv")
    summary_rows = read_csv(results / "experiment_b_summary.csv")
    benchmark_rows = [
        row
        for row in read_csv(results / "experiment_b_benchmark.csv")
        if as_int(row, "valid") == 1
    ]

    allocator_order = ["fixed_pool", "malloc", "bump"]
    benchmark_by_allocator: dict[str, list[float]] = defaultdict(list)
    for row in benchmark_rows:
        benchmark_by_allocator[row["allocator"]].append(
            as_float(row, "operations_per_second") / 1e6
        )
    benchmark_stats = {
        allocator: quantile_band(benchmark_by_allocator[allocator])
        for allocator in allocator_order
    }

    operations_by_allocator: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in operation_rows:
        operations_by_allocator[row["allocator"]].append(row)

    figure, axes = plt.subplots(1, 2, figsize=(12, 5.2))
    figure.suptitle(
        "Experiment B — uniform-block allocator comparison",
        fontsize=15,
        fontweight="bold",
        x=0.06,
        ha="left",
    )

    medians = [benchmark_stats[name][1] for name in allocator_order]
    lower = [benchmark_stats[name][1] - benchmark_stats[name][0] for name in allocator_order]
    upper = [benchmark_stats[name][2] - benchmark_stats[name][1] for name in allocator_order]
    axes[0].bar(
        allocator_order,
        medians,
        color=[COLORS[name] for name in allocator_order],
        yerr=np.asarray([lower, upper]),
        capsize=4,
    )
    axes[0].set_title("Replay throughput", loc="left")
    axes[0].set_ylabel("Million operations / second")
    axes[0].grid(axis="y")
    axes[0].text(
        0,
        -0.2,
        "Bars: median. Error bars: interquartile range.",
        transform=axes[0].transAxes,
        color="#555555",
    )

    for allocator in allocator_order:
        allocator_rows = operations_by_allocator[allocator]
        x = [as_int(row, "operation_index") for row in allocator_rows]
        retained_kib = [as_int(row, "retained_bytes") / 1024 for row in allocator_rows]
        axes[1].plot(
            x,
            retained_kib,
            label=allocator,
            color=COLORS[allocator],
            linewidth=1.8,
        )
    first_rows = operations_by_allocator[allocator_order[0]]
    axes[1].plot(
        [as_int(row, "operation_index") for row in first_rows],
        [as_int(row, "live_requested_bytes") / 1024 for row in first_rows],
        label="live requested",
        color="#202020",
        linewidth=1.3,
        linestyle="--",
    )
    axes[1].set_title("Retained bytes during the trace", loc="left")
    axes[1].set_xlabel("Operation index")
    axes[1].set_ylabel("KiB")
    axes[1].grid(axis="y")
    axes[1].legend()

    figure.tight_layout(rect=(0.02, 0.03, 0.99, 0.92))
    figure.savefig(output / "experiment_b_comparison.png", dpi=180)
    plt.close(figure)

    ranking = sorted(
        allocator_order,
        key=lambda allocator: benchmark_stats[allocator][1],
        reverse=True,
    )
    return {
        "summary_rows": summary_rows,
        "benchmark_stats": benchmark_stats,
        "ranking": ranking,
    }


def fmt_bytes(value: int) -> str:
    if value >= 1024 * 1024:
        return f"{value / (1024 * 1024):.2f} MiB"
    if value >= 1024:
        return f"{value / 1024:.2f} KiB"
    return f"{value} B"


def metric_at_fraction(values: np.ndarray, fraction: float) -> float:
    index = min(len(values) - 1, max(0, int(round((len(values) - 1) * fraction))))
    return float(values[index])


def write_report(results: Path, a: dict[str, object], b: dict[str, object]) -> None:
    metadata = json.loads((results / "run_metadata.json").read_text(encoding="utf-8"))
    summaries: list[dict[str, str]] = a["summaries"]  # type: ignore[assignment]
    counts: Counter[int] = a["counts"]  # type: ignore[assignment]
    allocator_counts: Counter[int] = a["allocator_counts"]  # type: ignore[assignment]
    qualifying = int(a["qualifying"])
    no_event = int(a["no_event"])
    median_internal: np.ndarray = a["median_internal"]  # type: ignore[assignment]
    median_external: np.ndarray = a["median_external"]  # type: ignore[assignment]

    if qualifying:
        maximum = max(counts.values())
        modal_classes = sorted(request_class for request_class, count in counts.items() if count == maximum)
        hypothesis = "SUPPORTED" if 12 in modal_classes else "FALSIFIED"
        modal_text = ", ".join(str(value) for value in modal_classes)
        modal_count = maximum
        predicted_count = counts.get(12, 0)
        allocator_maximum = max(allocator_counts.values())
        modal_allocator_classes = sorted(
            request_class
            for request_class, count in allocator_counts.items()
            if count == allocator_maximum
        )
        modal_allocator_text = ", ".join(str(value) for value in modal_allocator_classes)
    else:
        hypothesis = "INCONCLUSIVE"
        modal_text = "none"
        modal_allocator_text = "none"
        modal_count = 0
        predicted_count = 0

    layouts_valid = all(as_int(row, "layout_valid") == 1 for row in summaries)
    bug_failures = sum(as_int(row, "bug_or_policy_failures") for row in summaries)
    capacity_failures = sum(as_int(row, "capacity_failures") for row in summaries)
    fragmentation_failures = sum(as_int(row, "fragmentation_failures") for row in summaries)

    internal_start = metric_at_fraction(median_internal, 0.1)
    internal_middle = metric_at_fraction(median_internal, 0.5)
    internal_end = metric_at_fraction(median_internal, 0.9)
    external_start = metric_at_fraction(median_external, 0.1)
    external_middle = metric_at_fraction(median_external, 0.5)
    external_end = metric_at_fraction(median_external, 0.9)

    benchmark_stats: dict[str, tuple[float, float, float]] = b["benchmark_stats"]  # type: ignore[assignment]
    ranking: list[str] = b["ranking"]  # type: ignore[assignment]
    summary_rows: list[dict[str, str]] = b["summary_rows"]  # type: ignore[assignment]
    predicted_ranking = ["bump", "fixed_pool", "malloc"]
    throughput_verdict = "SUPPORTED" if ranking == predicted_ranking else "FALSIFIED"

    lines = [
        "# Allocator profiling report",
        "",
        f"Generated on `{platform.platform()}` with Python `{platform.python_version()}`.",
        "",
        "## Executive verdict",
        "",
        f"- Largest-class hypothesis: **{hypothesis}**. Class 12 was the registered prediction; "
        f"the modal first-failing class was {modal_text} across {qualifying} qualifying traces "
        f"({no_event} had no fragmentation-induced failure). The modal count was {modal_count}; "
        f"class 12 occurred {predicted_count} times.",
        f"- Throughput-order prediction: **{throughput_verdict}**. Measured median order: "
        + " > ".join(ranking)
        + ".",
        f"- Correctness during profiling: **{'PASS' if layouts_valid and bug_failures == 0 else 'FAIL'}**. "
        f"Observed {bug_failures} allocator-policy/bug failures.",
        "",
        "## Experiment A — varied-size segregated pool",
        "",
        f"Protocol: {metadata['experiment_a']['seeds']} seeds × "
        f"{metadata['experiment_a']['operations_per_trace']} operations, "
        f"{fmt_bytes(metadata['experiment_a']['arena_size_bytes'])} arena. "
        "Every fourth operation frees the oldest logical allocation.",
        "",
        f"The run recorded {fragmentation_failures} fragmentation failures and "
        f"{capacity_failures} ordinary capacity failures. The categorical result uses only "
        "the first fragmentation failure in each seeded trace.",
        f"The modal sampled request class was {modal_text}; after allocator metadata and "
        f"alignment, the modal free-list class index was {modal_allocator_text}. "
        "The second view is reported because raw request classes and allocator block classes "
        "do not map one-to-one.",
        "The categorical grade follows the predeclared modal-first aggregation rule. The counts "
        "are dispersed, so this falsifies the exact registered call on this protocol but is not "
        "strong evidence that the observed modal class is universally privileged.",
        "",
        "| Curve checkpoint | 10% of trace | 50% of trace | 90% of trace |",
        "|---|---:|---:|---:|",
        f"| Median internal waste | {fmt_bytes(round(internal_start))} | "
        f"{fmt_bytes(round(internal_middle))} | {fmt_bytes(round(internal_end))} |",
        f"| Median external score | {external_start:.3f} | {external_middle:.3f} | "
        f"{external_end:.3f} |",
        "",
        "External fragmentation is `1 - largest_free_extent / total_free_extent`. "
        "A failure is labeled fragmentation only when aggregate free extent is large enough "
        "but the largest contiguous extent is too small for the required block size.",
        "",
        "## Experiment B — uniform-block comparison",
        "",
        f"Protocol: {metadata['experiment_b']['operations']} operations at "
        f"{metadata['experiment_b']['block_size_bytes']} bytes per request; "
        f"fixed and bump arena size {fmt_bytes(metadata['experiment_b']['arena_size_bytes'])}. "
        "The malloc baseline uses the system heap.",
        "",
        "| Allocator | Median Mops/s | IQR Mops/s | First failure | Successful allocs | "
        "Peak retained | Final internal waste | Final unreclaimed dead |",
        "|---|---:|---:|---:|---:|---:|---:|---:|",
    ]

    summary_by_name = {row["allocator"]: row for row in summary_rows}
    for allocator in ["fixed_pool", "malloc", "bump"]:
        low, median, high = benchmark_stats[allocator]
        row = summary_by_name[allocator]
        first_failure = as_int(row, "first_failure_op")
        lines.append(
            f"| {allocator} | {median:.3f} | {low:.3f}–{high:.3f} | "
            f"{first_failure if first_failure else 'none'} | "
            f"{as_int(row, 'successful_allocations')} | "
            f"{fmt_bytes(as_int(row, 'peak_retained'))} | "
            f"{fmt_bytes(as_int(row, 'final_internal_waste'))} | "
            f"{fmt_bytes(as_int(row, 'final_unreclaimed_dead'))} |"
        )

    lines.extend(
        [
            "",
            "## Build-vs-buy interpretation",
            "",
            "For a uniform KV-style block contract with arbitrary individual frees, the fixed pool "
            "is the structurally matched custom allocator: it reuses any returned block and has no "
            "external-fragmentation mechanism. The bump allocator's timing is meaningful, but its "
            "individual `free` is a no-op; its growing dead-byte curve is the cost. `malloc` remains "
            "the production general-purpose baseline and should not be judged on portable external "
            "fragmentation here because the harness cannot inspect its private heap extents.",
            "",
            "This licenses a workload-specific component verdict, not general allocator superiority.",
            "",
            "## Lab-log delta",
            "",
            f"- Registered categorical call: class 12 first. Result: {hypothesis.lower()} "
            f"(modal class {modal_text}).",
            f"- Internal-waste direction: median checkpoints were {fmt_bytes(round(internal_start))}, "
            f"{fmt_bytes(round(internal_middle))}, and {fmt_bytes(round(internal_end))}.",
            f"- External-curve checkpoints were {external_start:.3f}, {external_middle:.3f}, "
            f"and {external_end:.3f}; inspect the plotted IQR before assigning a smooth shape.",
            f"- Directional throughput call: bump > fixed_pool > malloc. Result: "
            f"{throughput_verdict.lower()} ({' > '.join(ranking)}).",
            "",
            "## Files",
            "",
            "- `experiment_a_operations.csv`: one row per operation per seed.",
            "- `experiment_a_seed_summary.csv`: first qualifying failure and failure counts per seed.",
            "- `experiment_b_operations.csv`: memory behavior for each allocator.",
            "- `experiment_b_benchmark.csv`: every timing repetition.",
            "- `experiment_b_summary.csv`: behavior summary.",
            "- PNG files: publication-ready plots generated from those CSVs.",
            "",
        ]
    )
    (results / "report.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot allocator profiling CSV outputs")
    parser.add_argument("results", type=Path, nargs="?", default=Path("results"))
    args = parser.parse_args()
    results = args.results.resolve()
    set_style()
    experiment_a = plot_experiment_a(results, results)
    experiment_b = plot_experiment_b(results, results)
    write_report(results, experiment_a, experiment_b)
    print(f"wrote plots and report to {results}")


if __name__ == "__main__":
    main()
