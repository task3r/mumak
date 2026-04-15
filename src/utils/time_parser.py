#!/usr/bin/env python3

import numpy as np
import sys

def parse_and_analyze(filename):
    event_times = {}

    with open(filename, "r") as file:
        for line in file:
            parts = line.split()
            if len(parts) != 2:
                continue
            event_type, value = parts
            try:
                value = int(value)
            except ValueError:
                continue

            if event_type not in event_times:
                event_times[event_type] = []
            event_times[event_type].append(value)

    percentile_ranges = [0, 25, 50, 75, 90, 95, 99, 100]

    def compute_stats(values):
        if not values:
            return None

        values = np.sort(values)
        total_time = np.sum(values) / 1000000

        percentiles = [np.percentile(values, p) for p in percentile_ranges[1:]]

        time_in_ranges = []
        prev_idx = 0
        for p in percentile_ranges[1:]:
            idx = int(len(values) * (p / 100.0))
            time_in_ranges.append(np.sum(values[prev_idx:idx]) / 1000000)
            prev_idx = idx

        return total_time, percentiles, time_in_ranges

    results = {event: compute_stats(times) for event, times in event_times.items()}

    print(f"{'Event':<6} {'Total Time (s)':>14} {'Count':>8} {'Median':>10} {'p75':>10} {'p90':>10} {'p95':>10} {'p99':>10}")
    print("-" * 90)
    for event, stats in sorted(results.items()):
        if stats is None:
            continue
        total_time, percentiles, _ = stats
        _, _, median, p75, p90, p95, p99 = percentiles
        count = len(event_times[event])
        print(f"{event:<6} {total_time:14.2f} {count:8} {median:10.2f} {p75:10.2f} {p90:10.2f} {p95:10.2f} {p99:10.2f}")

    print("\nTime spent in each percentile range:")
    print(f"{'Event':<6} {'0-25%':>12} {'25-50%':>12} {'50-75%':>12} {'75-90%':>12} {'90-95%':>12} {'95-99%':>12} {'99-100%':>12}")
    print("-" * 100)
    for event, stats in sorted(results.items()):
        if stats is None:
            continue
        _, _, time_in_ranges = stats
        print(f"{event:<6} " + " ".join(f"{t:12.2f}" for t in time_in_ranges))

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <filename>")
        sys.exit(1)

    parse_and_analyze(sys.argv[1])
