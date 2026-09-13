#!/bin/bash

# Launch hyperfine for each benchmark exe

set -euo pipefail

benchname="ctor-dtor"

benchmarks=(
    mutex
    vyukov-idle
    vyukov-spin
    work-stealing
)

cmds=()

for bench in "${benchmarks[@]}"; do
    cmds+=("build/benchmarks/$benchname/$bench/$benchname-$bench")
done

if [ $# -eq 1 ]; then
	hyperfine "${cmds[@]}" --runs $1
	exit 0
fi

hyperfine "${cmds[@]}" --runs 10000
