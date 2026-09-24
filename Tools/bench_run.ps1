# Drive a benchmark: run one or two samples alternately, and file away what
# each run wrote. The frame counts, warmup and output paths live in the
# -Config document, so this script only orchestrates and collects.
#
# Alternating A and B is the point when comparing two builds or two APIs:
# it cancels the thermal drift and background load that a run of five A's
# followed by five B's would bake into the second half.
#
# usage: bench_run.ps1 [-AppA <exe>] [-AppB <exe>] [-Reps 5]
#                      [-StageDir bench] [-OutDir bench-runs]
#                      [-Config Tools/bench.app.json]
#
# Every argument falls back to the default below. The sample must be a
# Debug or CROWY_BENCHMARK build, run with --config; the config's report and
# frame paths must point into -StageDir.
#
# Run from the repository root: samples load Engine/Shader and Content by
# relative path.
param(
    [string]$AppA = "",
    [string]$AppB = "",
    [int]$Reps = 5,
    # where the samples write their report and CSV
    [string]$StageDir = "bench",
    # where this script keeps each run
    [string]$OutDir = "bench-runs",
    # the app document every run is started with
    [string]$Config = "Tools/bench.app.json",
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = "Stop"

$apps = @()
foreach ($app in @($AppA, $AppB)) {
    if (-not $app) { continue }

    if (-not (Test-Path $app)) {
        Write-Host "FAIL: no such executable: $app"
        exit 1
    }
    $apps += $app
}

if ($apps.Count -eq 0) {
    Write-Host "FAIL: give at least one executable"
    exit 1
}

if (-not (Test-Path $Config -PathType Leaf)) {
    Write-Host "FAIL: no such config: $Config"
    exit 1
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

for ($rep = 1; $rep -le $Reps; $rep++) {
    foreach ($app in $apps) {
        $name = [IO.Path]::GetFileNameWithoutExtension($app)
        $label = "$name-rep$rep"

        # start from an empty stage so the collection below cannot pick up
        # anything an earlier run left behind
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $StageDir
        New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

        Write-Host "running $label ..."
        $proc = Start-Process -FilePath $app `
            -ArgumentList "--config", "`"$Config`"" `
            -WorkingDirectory (Get-Location) `
            -NoNewWindow -PassThru
        $null = $proc.Handle

        if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            $proc.WaitForExit() | Out-Null

            Write-Host "FAIL: $label ran past $TimeoutSeconds s."
            Write-Host "      is this a Debug or CROWY_BENCHMARK build, run with --config?"
            exit 1
        }

        if ($proc.ExitCode -ne 0) {
            Write-Host "FAIL: $label exited with status $($proc.ExitCode)"
            exit 1
        }

        $produced = @(Get-ChildItem -File -ErrorAction SilentlyContinue $StageDir)
        if ($produced.Count -eq 0) {
            Write-Host "FAIL: $label wrote nothing into $StageDir"
            Write-Host "      do $Config's report_path and frame_path point in there?"
            exit 1
        }

        $runDir = Join-Path $OutDir $label
        New-Item -ItemType Directory -Force -Path $runDir | Out-Null
        $produced | Move-Item -Destination $runDir -Force

        Write-Host "  -> $runDir ($($produced.Count) file(s))"
    }
}

Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $StageDir
Write-Host "done: $Reps rep(s) of $($apps.Count) app(s) under $OutDir"
exit 0
