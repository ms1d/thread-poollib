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



base_path='benchmarks/box-muller'
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

vec_sizes=(
	992
	10016

	100000
	400000

	1000000
	4000000
)

# Likely the least important for these specific benchmarks - keeping it at 4 and 4096 for now
num_tasks=(
	4
	#16
	#64
	#256
	#1024
	4096
)

inner_loops=(
	1
	50
)


if [[ "$delete" == true ]]; then
    rm -rf "$data_path"
	echo "Deleted existing temp data at data path ($data_path)"
fi

mkdir -p "$data_path" "$results_path"

for tasks in "${num_tasks[@]}"; do
	for vec in "${vec_sizes[@]}"; do
		for threads in "${num_threads[@]}"; do
			for loop in "${inner_loops[@]}"; do
				FILE="$data_path/dump.$tasks.$vec.$threads.$loop.txt"
				TMP="$data_path/tmp.txt"

				if [ -f "$FILE" ]; then
					echo "skipping: "$FILE" (already exists)"
				else
					: > "$TMP"
					printf "\nNUM_THREADS: %d NUM_TASKS: %d VEC_SIZE: %d INNER_LOOPS: %d\n" "$threads" "$tasks" "$vec" "$loop" >> "$TMP"

					printf "Compiling (num threads = %d, num tasks = %d, vec_size = %d inner_loops = %d)...\n" "$threads" "$tasks" "$vec" "$loop"
					cmake \
						-S . \
						-B build \
						-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
						-DBUILD_BENCHMARKS=ON \
						-DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
						-DNUM_THREADS=$threads \
						-DNUM_TASKS=$tasks \
						-DVEC_SIZE=$vec \
						-DINNER_LOOPS=$loop > /dev/null || exit 1

					cmake --build build --parallel > /dev/null || exit 2

					rm -f /tmp/tmp.json

					"$scripts_path/launch.sh" $(($runs / $loop))
					cat /tmp/tmp.json >> "$TMP"

					echo "BENCHMARKS COMPLETE" >> "$TMP"
					mv "$TMP" "$FILE"
				fi
			done
		done
	done
done

echo "BENCHMARKS COMPLETE"

# Generate artifacts

python3 "$scripts_path/parse.py" -o "$data_path" "$data_path" &&
    python3 "$scripts_path/plot.py" "$data_path/benchmarks.json" -o "$results_path"
