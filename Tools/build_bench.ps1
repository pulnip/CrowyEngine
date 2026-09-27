# Builds the benchmark tree for Tools/bench_run.ps1: Tools/build.ps1 with the
# Windows-llvm-x64-Bench preset, which turns CROWY_BENCHMARK on in a tree of
# its own (build-bench/), in Release, where no debug layer runs. Every
# argument is build.ps1's.
#
# build.ps1 looks for the cache and keeps its logs and status under build/
# whatever the preset, so the first bench build passes -Fresh, and a bench
# build's log replaces the Debug tree's.
#
#   Tools/build_bench.ps1 -Target Playground -Fresh -Detach   # the first
#   Tools/build_bench.ps1 -Target Playground -Detach
#   Tools/build.ps1 -Status
param(
    [string[]] $Target,
    [switch]   $Clean,
    [switch]   $Fresh,
    [switch]   $Detach,
    [switch]   $Status
)

& (Join-Path $PSScriptRoot 'build.ps1') `
    -Preset 'Windows-llvm-x64-Bench' -Config 'Release' @PSBoundParameters
exit $LASTEXITCODE
