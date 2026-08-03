# Smoke-run a sample: launch it, let it render for a while,
# then fail on crashes, thrown exceptions, or D3D12 validation errors.
# An early exit with status 0 counts as success (e.g. HelloCompute).
#
# usage: smoke_run.ps1 <executable> [duration-seconds] [capture-image-path]
#
# Same contract as Tools/smoke_run.sh:
#   duration falls back to $env:CROWY_SMOKE_DURATION, then 5 seconds.
#   a capture path falls back to $env:CROWY_SMOKE_CAPTURE_DIR\<sample>.bmp
#   when that directory variable is set; capture rides on the engine's
#   CROWY_DUMP_FRAME hook, frame index override via CROWY_SMOKE_CAPTURE_AT.
#
# Validation errors: the script sets CROWY_D3D_DEBUG_BREAK=1, which makes
# the engine break on debug-layer errors — without a debugger that aborts
# the process, which this script reports as a failure. Debug builds only
# (the debug layer is compiled out of release).
#
# Run from the repository root: samples load Engine/Shader and Content
# by relative path.
param(
    [Parameter(Mandatory = $true)][string]$App,
    [int]$Duration = 0,
    [string]$Capture = ""
)

$ErrorActionPreference = "Stop"

if ($Duration -le 0) {
    $Duration = 5
    if ($env:CROWY_SMOKE_DURATION) {
        $Duration = [int]$env:CROWY_SMOKE_DURATION
    }
}

if (-not $Capture -and $env:CROWY_SMOKE_CAPTURE_DIR) {
    $sampleName = [IO.Path]::GetFileNameWithoutExtension($App)
    $Capture = Join-Path $env:CROWY_SMOKE_CAPTURE_DIR "$sampleName.bmp"
}

$outLog = Join-Path $env:TEMP "crowy-smoke-$PID-out.log"
$errLog = Join-Path $env:TEMP "crowy-smoke-$PID-err.log"

$env:CROWY_D3D_DEBUG_BREAK = "1"

if ($Capture) {
    $captureDir = Split-Path -Parent $Capture
    if ($captureDir) {
        New-Item -ItemType Directory -Force -Path $captureDir | Out-Null
    }
    Remove-Item -Force -ErrorAction SilentlyContinue $Capture
    $env:CROWY_DUMP_FRAME = $Capture
    if ($env:CROWY_SMOKE_CAPTURE_AT) {
        $env:CROWY_DUMP_FRAME_AT = $env:CROWY_SMOKE_CAPTURE_AT
    }
}

$proc = Start-Process -FilePath $App `
    -WorkingDirectory (Get-Location) `
    -RedirectStandardOutput $outLog `
    -RedirectStandardError $errLog `
    -NoNewWindow -PassThru

# cache the process handle right away: without this, ExitCode reads back
# null when the process exits before we query it (fast headless samples)
$null = $proc.Handle

$status = 0
if ($proc.WaitForExit($Duration * 1000)) {
    $status = $proc.ExitCode
    if ($status -ne 0) {
        Write-Host "FAIL: exited early with status $status"
    }
}
else {
    # still alive after the watch window: healthy. shut it down.
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    $proc.WaitForExit() | Out-Null
}

$log = @()
foreach ($f in @($outLog, $errLog)) {
    if (Test-Path $f) {
        $log += Get-Content $f
    }
}

$markers = "D3D12 ERROR|failed assertion|Assertion failed|Exception"
if ($log | Select-String -Pattern $markers) {
    Write-Host "FAIL: assertion or validation error"
    $status = 1
}

if ($status -ne 0) {
    Write-Host "--- last log lines ---"
    $log | Select-Object -Last 40 | ForEach-Object { Write-Host $_ }
    Remove-Item -Force -ErrorAction SilentlyContinue $outLog, $errLog
    exit 1
}

if ($Capture) {
    if (Test-Path $Capture) {
        Write-Host "captured frame: $Capture"
    }
    else {
        # headless samples have no swapchain, so nothing to dump
        Write-Host "note: no frame captured (sample presented no frame?)"
    }
}

Remove-Item -Force -ErrorAction SilentlyContinue $outLog, $errLog
exit 0
