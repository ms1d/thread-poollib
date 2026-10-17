#!/usr/bin/env python3

# ----- AI USAGE DISCLOSURE ----
# This script was generated with assistance from OpenAI's ChatGPT.
# USAGE:
#   python parse_benchmarks.py /path/to/dump.txt -o /path/to/output
#   python parse_benchmarks.py /path/to/dump_directory -o /path/to/output
#
# A directory input processes all dump.*.*.*.txt files in sorted order.

import argparse
import csv
import json
import math
import re
import sys
from pathlib import Path

CONFIG_RE = re.compile(
    r"NUM_THREADS:\s*(\d+)\s+"
    r"NUM_TASKS:\s*(\d+)\s+"
    r"VEC_SIZE:\s*(\d+)"
)

TIME_FIELDS = [
    "time_wall_clock_unit",
    "time_wall_clock_mean",
    "time_wall_clock_stddev",
    "time_wall_clock_median",
    "time_wall_clock_min",
    "time_wall_clock_max",
    "time_wall_clock_iqr",
]

MEMORY_FIELDS = [
    "memory_peak_resident_unit",
    "memory_peak_resident_mean",
    "memory_peak_resident_stddev",
    "memory_peak_resident_median",
    "memory_peak_resident_min",
    "memory_peak_resident_max",
    "memory_peak_resident_iqr",
]

OUTPUT_FIELDS = [
    "executable",
    "num_threads",
    "num_tasks",
    "vec_size",
    *TIME_FIELDS,
    *MEMORY_FIELDS,
]


def executable_name(command):
    """Extract the executable name from a benchmark command."""
    # Handles commands containing arguments as well as bare executable paths.
    import shlex

    try:
        parts = shlex.split(command)
    except ValueError:
        return ""

    if not parts:
        return ""

    return Path(parts[0]).name


def parse_file(input_path):
    results = {
        "ctor_dtor": [],
        "vector_add": [],
    }

    config = None
    section = None

    with input_path.open("r", newline="", encoding="utf-8-sig") as file:
        for line in file:
            line = line.strip()

            if not line:
                continue

            match = CONFIG_RE.search(line)
            if match:
                config = {
                    "num_threads": int(match.group(1)),
                    "num_tasks": int(match.group(2)),
                    "vec_size": int(match.group(3)),
                }
                continue

            if line == "CTOR DTOR":
                section = "ctor_dtor"
                continue

            if line == "VECTOR ADD":
                section = "vector_add"
                continue

            if line.startswith("BENCHMARKS COMPLETE"):
                section = None
                continue

            if section is None or config is None:
                continue

            try:
                row = next(csv.reader([line]))
            except csv.Error:
                continue

            # Hyperfine CSV rows contain more columns than we need.
            if len(row) < 13 or not row[0].startswith("build/"):
                continue

            record = {
                "executable": executable_name(row[0]),
                **config,
            }

            for field, value in zip(TIME_FIELDS[:6], row[1:7]):
                record[field] = value

            for field, value in zip(MEMORY_FIELDS[:6], row[7:13]):
                record[field] = value

            results[section].append(record)

    return results


def percentile(values, p):
    values = sorted(values)

    if not values:
        raise ValueError("Cannot calculate percentile of empty data")

    index = (len(values) - 1) * p
    lower = math.floor(index)
    upper = math.ceil(index)

    if lower == upper:
        return values[lower]

    fraction = index - lower
    return values[lower] * (1 - fraction) + values[upper] * fraction


def calculate_iqr(values):
    return percentile(values, 0.75) - percentile(values, 0.25)


def parse_json_measurements(input_path):
    text = input_path.read_text(encoding="utf-8-sig")
    decoder = json.JSONDecoder()

    json_starts = re.finditer(
        r'(?m)^\{\s*"schema_version"\s*:',
        text,
    )

    results = {}

    for match in json_starts:
        try:
            data, _ = decoder.raw_decode(text, match.start())
        except json.JSONDecodeError as exc:
            print(
                f"Warning: invalid JSON in {input_path}: {exc}",
                file=sys.stderr,
            )
            continue

        for benchmark in data.get("results", []):
            command = benchmark.get("command", "")
            measurements = benchmark.get("measurements", [])
            name = executable_name(command)

            if not name:
                continue

            metrics = {}

            for metric in ("time_wall_clock", "memory_peak_resident"):
                values = [
                    measurement[metric]["value"]
                    for measurement in measurements
                    if metric in measurement
                    and "value" in measurement[metric]
                ]

                metrics[f"{metric}_iqr"] = (
                    calculate_iqr(values) if values else ""
                )

            results[name] = metrics

    return results


def parse_inputs(input_path):
    """Parse one log file or aggregate every matching fragment in a directory."""
    if input_path.is_file():
        files = [input_path]
    elif input_path.is_dir():
        files = sorted(input_path.glob("dump.*.*.*.txt"))

        if not files:
            raise ValueError(
                f"No dump.*.*.*.txt files found in {input_path}"
            )
    else:
        raise ValueError(f"Input does not exist: {input_path}")

    aggregated = {
        "ctor_dtor": [],
        "vector_add": [],
    }

    for path in files:
        print(f"Parsing {path}", file=sys.stderr)

        parsed = parse_file(path)

        try:
            iqr_results = parse_json_measurements(path)
        except OSError as exc:
            print(f"Warning: cannot read JSON data in {path}: {exc}",
                  file=sys.stderr)
            iqr_results = {}

        for records in parsed.values():
            for record in records:
                metrics = iqr_results.get(record["executable"], {})
                record.update(metrics)

        for section, records in parsed.items():
            aggregated[section].extend(records)

    return aggregated, len(files)


def write_csv(path, records):
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=OUTPUT_FIELDS)
        writer.writeheader()
        writer.writerows(records)


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Parse benchmark logs or aggregate fragmented benchmark logs "
            "into CSV files."
        )
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Input log file or directory containing dump.*.*.*.txt files",
    )
    parser.add_argument(
        "-o",
        "--out-dir",
        type=Path,
        help="Output directory (default: directory containing the input)",
    )
    args = parser.parse_args()

    try:
        results, file_count = parse_inputs(args.input)
    except (ValueError, OSError) as exc:
        parser.error(str(exc))

    output_dir = args.out_dir or (
        args.input if args.input.is_dir() else args.input.parent
    )
    output_dir.mkdir(parents=True, exist_ok=True)

    for section, records in results.items():
        output_path = output_dir / f"{section}.csv"
        write_csv(output_path, records)
        print(f"{output_path}: {len(records)} benchmark rows")

    print(f"Processed {file_count} log file(s)")


if __name__ == "__main__":
    main()
