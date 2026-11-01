#!/bin/bash

# Script to build, setup and run benchmarks
# Writes to stdout; ideally should be piped to a file
# MUST BE RUN IN REPO ROOT!

set -euo pipefail

cat include/mpmc.hpp > /dev/null || exit -1



delete=false

while getopts ":d" opt; do
    case "$opt" in
        d) delete=true ;;
        *)
            printf "Usage: %s [-d] <runs>\n" "$0" >&2
            exit 2
            ;;
    esac
done

shift $((OPTIND - 1))

if [[ $# -ne 1 ]]; then
    printf "Usage: %s [-d] <runs>\n" "$0" >&2
    exit 2
fi

runs=$1



base_path='benchmarks/ctor-dtor'
data_path="$base_path/data"
results_path="$base_path/results"
scripts_path="$base_path/scripts"

runs=$1

num_threads=(
	1
	2
	4
	8
	16
	32
)

num_tasks=(
	4
	16
	64
	256
	1024
	4096
)



if [[ "$delete" == true ]]; then
    rm -f "$data_path"
	echo "Deleted existing temp data at data path ($data_path)"
fi

mkdir -p "$data_path" "$results_path"

for tasks in "${num_tasks[@]}"; do
	for threads in "${num_threads[@]}"; do

		FILE="$data_path/dump.$tasks.$threads.txt"
		TMP="$data_path/tmp.txt"

		if [ -f "$FILE" ]; then
			echo "skipping: "$FILE" (already exists)"
		else
			: > "$TMP"
			printf "\nNUM_THREADS: %d NUM_TASKS: %d\n" "$threads" "$tasks" >> "$TMP"

			printf "Compiling (num threads = %d, num tasks = %d)...\n" "$threads" "$tasks"
			cmake \
				-S . \
				-B build \
				-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
				-DBUILD_BENCHMARKS=ON \
				-DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
				-DNUM_THREADS=$threads \
				-DNUM_TASKS=$tasks > /dev/null || exit 1

			cmake --build build > /dev/null || exit 2

			rm -f /tmp/tmp.json

			"$scripts_path/launch.sh" $runs
			cat /tmp/tmp.json >> "$TMP"

			echo "BENCHMARKS COMPLETE" >> "$TMP"
			mv "$TMP" "$FILE"
		fi
	done
done

echo "BENCHMARKS COMPLETE"

# Generate artifacts

python3 "$scripts_path/parse.py" -o "$data_path" "$data_path" &&
    python3 "$scripts_path/plot.py" "$data_path/benchmarks.json" -o "$results_path"
