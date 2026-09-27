#!/bin/sh
# Builds the benchmark tree for Tools/bench_run.sh: Tools/build.sh with the
# macOS-brew-llvm-Bench preset, which turns CROWY_BENCHMARK on in a tree of
# its own (build-bench/), in Release. Every argument is build.sh's.
#
# build.sh looks for the cache and keeps its logs and status under build/
# whatever the preset, so the first bench build passes --fresh, and a bench
# build's log replaces the Debug tree's.
#
#   Tools/build_bench.sh --target Playground --fresh --detach   # the first
#   Tools/build_bench.sh --target Playground --detach
#   Tools/build.sh --status
exec sh "$(dirname "$0")/build.sh" --preset macOS-brew-llvm-Bench --config Release "$@"
