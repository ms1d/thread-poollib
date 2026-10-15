#!/bin/bash

# Launch hyperfine for each benchmark exe

set -euo pipefail

benchname="vector-add"

benchmarks=(
	single-threaded
    mutex
    vyukov-idle
    vyukov-spin
    work-stealing
	manual
)

cmds=()

for bench in "${benchmarks[@]}"; do
    cmds+=("build/benchmarks/$benchname/$bench/$benchname-$bench")
done

runs=10000

if [ $# -eq 1 ]; then
	runs=$1
fi

hyperfine "${cmds[@]}" --runs $runs -N --warmup $(($runs / 20)) --export-csv /tmp/test.csv > /dev/null
cat /tmp/test.csv
rm -f /tmp/test.csv
