#!/bin/bash

# Script to build, setup and run benchmarks
# Writes to stdout; ideally should be piped to a file
# MUST BE RUN IN REPO ROOT!

set -euo pipefail

cat include/mpmc.hpp > /dev/null || exit -1

if [[ $# -ne 1 ]]; then
	printf "Usage: %s <runs>\n" "$0" >&2
	exit -2
fi

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
	#1000000
	#10000000
	#25000000
	#50000000
	#100000000
	#200000000
)

# Likely the least important for these specific benchmarks - keeping it at 64
num_tasks=(
	#4
	#16
	64
	#256
	#1024
	#4096
)

for tasks in "${num_tasks[@]}"; do
	for vec in "${vec_sizes[@]}"; do
		for threads in "${num_threads[@]}"; do

			FILE="dump.$tasks.$vec.$threads.txt"
			TMP="tmp.txt"

			if [ -f "$FILE" ]; then
				echo "skipping: "$FILE" (already exists)"
			else
				rm -f "$TMP"
				touch "$TMP"
				printf "\nNUM_THREADS: %d NUM_TASKS: %d VEC_SIZE: %d\n" "$threads" "$tasks" "$vec" >> "$TMP"

				printf "Compiling (num threads = %d, num tasks = %d, vec size = %d)...\n" "$threads" "$tasks" "$vec"
				cmake \
					-S . \
					-B build \
					-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
					-DBUILD_BENCHMARKS=ON \
					-DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
					-DNUM_THREADS=$threads \
					-DNUM_TASKS=$tasks \
					-DVEC_SIZE=$vec > /dev/null || exit 1

				cmake --build build > /dev/null || exit 2

				cp benchmarks/ctor-dtor/launch.sh build/benchmarks/ctor-dtor
				cp benchmarks/vector-add/launch.sh build/benchmarks/vector-add

				echo "CTOR DTOR" >> "$TMP"
				rm -f /tmp/tmp.json /tmp/tmp.csv

				build/benchmarks/ctor-dtor/launch.sh $runs
				cat /tmp/tmp.csv >> "$TMP"
				cat /tmp/tmp.json >> "$TMP"

				echo "VECTOR ADD" >> "$TMP"
				rm -f /tmp/tmp.json /tmp/tmp.csv

				build/benchmarks/vector-add/launch.sh $runs
				cat /tmp/tmp.csv >> "$TMP"
				cat /tmp/tmp.json >> "$TMP"

				echo "BENCHMARKS COMPLETE" >> "$TMP"
				cat "$TMP" > "$FILE"
			fi
		done
	done
done

echo "BENCHMARKS COMPLETE; RESTORING DEFAULTS"

cmake \
	-S . \
	-B build \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-DBUILD_BENCHMARKS=ON || exit 3

cmake --build build || exit 4

# Generate artifacts

python3 benchmarks/scripts/parse_benchmarks.py ./ -o benchmarks/results &&
	mv benchmarks/results/ctor_dtor.csv benchmarks/ctor-dtor && mv benchmarks/results/vector_add.csv benchmarks/vector-add &&
	python3 benchmarks/scripts/plot_ctor_dtor.py benchmarks/ctor-dtor/ctor_dtor.csv -o benchmarks/ctor-dtor &&
	python3 benchmarks/scripts/plot_vector_add.py benchmarks/vector-add/vector_add.csv -o benchmarks/vector-add &&
	rm -r benchmarks/results &&
	rm dump*
