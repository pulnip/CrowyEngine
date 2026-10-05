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
#   a .bmp capture becomes a PNG through ImageCompareCheck.exe beside the
#   executable (the BMP stays when that tool is not built).
#   when Engine\*\Sample\Golden\<sample>.dx12.png (or Spike\Golden) exists,
#   frame 60 is captured (to %TEMP%\crowy-smoke without a capture
#   directory) and compared against it on every run, and a
#   difference fails the run with a heat map beside the capture; the
#   failure prints the Copy-Item that accepts the new picture.
#   with such a golden the run waits past the duration until frame 60's
#   capture is complete, up to $env:CROWY_SMOKE_CAPTURE_TIMEOUT seconds
#   (60), and fails when none lands: a golden never gates on a frame it
#   did not compare.
#   an exit status of 77 is a skip (the sample's content is missing), and
#   passes through for ctest's SKIP_RETURN_CODE.
#   with CROWY_SMOKE_MUST_EXIT=1 (checks) the program must exit by itself
#   within 60 s or the duration, whichever is longer.
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

$repoRoot = Split-Path -Parent $PSScriptRoot
$sampleName = [IO.Path]::GetFileNameWithoutExtension($App)
$tool = Join-Path (Split-Path -Parent $App) "ImageCompareCheck.exe"
$backend = "dx12"

# runs ImageCompareCheck.exe and returns its exit code; the tool reports
# errors on stderr, which must not stop this script
function Invoke-ImageCompare([string[]]$Arguments, [switch]$Quiet) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $lines = & $tool $Arguments
        if (-not $Quiet) {
            # foreach, not ForEach-Object: piping a silent run's $null would
            # print an empty line
            foreach ($line in $lines) {
                Write-Host $line
            }
        }
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

if ($Duration -le 0) {
    $Duration = 5
    if ($env:CROWY_SMOKE_DURATION) {
        $Duration = [int]$env:CROWY_SMOKE_DURATION
    }
}

if (-not $Capture -and $env:CROWY_SMOKE_CAPTURE_DIR) {
    $Capture = Join-Path $env:CROWY_SMOKE_CAPTURE_DIR "$sampleName.bmp"
}

# the golden a frame-60 capture is compared with, found before the launch so
# the watch can wait for that capture
$expectedGolden = $null
if (-not $env:CROWY_SMOKE_CAPTURE_AT -or $env:CROWY_SMOKE_CAPTURE_AT -eq "60") {
    $expectedGolden = Get-ChildItem -ErrorAction SilentlyContinue -Path @(
        (Join-Path $repoRoot "Engine\*\Sample\Golden\$sampleName.$backend.png"),
        (Join-Path $repoRoot "Engine\*\Spike\Golden\$sampleName.$backend.png")
    ) | Select-Object -First 1
}
# a golden gates every run, not only runs that collect captures
if (-not $Capture -and $expectedGolden) {
    $Capture = Join-Path $env:TEMP "crowy-smoke\$sampleName.bmp"
}
$mustExit = $env:CROWY_SMOKE_MUST_EXIT -eq "1"
$captureTimeout = 60
if ($env:CROWY_SMOKE_CAPTURE_TIMEOUT) {
    $captureTimeout = [int]$env:CROWY_SMOKE_CAPTURE_TIMEOUT
}

# a dump is written on a thread: complete once the BMP header's file size
# matches the bytes on disk
function Test-CaptureComplete([string]$Path) {
    if (-not $Path -or -not (Test-Path $Path)) {
        return $false
    }
    try {
        $stream = [IO.File]::Open($Path, "Open", "Read", "ReadWrite")
        try {
            $header = New-Object byte[] 6
            if ($stream.Read($header, 0, 6) -lt 6) {
                return $false
            }
            return [BitConverter]::ToUInt32($header, 2) -eq $stream.Length
        }
        finally {
            $stream.Dispose()
        }
    }
    catch {
        return $false
    }
}

$outLog = Join-Path $env:TEMP "crowy-smoke-$PID-out.log"
$errLog = Join-Path $env:TEMP "crowy-smoke-$PID-err.log"

$env:CROWY_D3D_DEBUG_BREAK = "1"

if ($Capture) {
    $captureDir = Split-Path -Parent $Capture
    if ($captureDir) {
        New-Item -ItemType Directory -Force -Path $captureDir | Out-Null
    }
    # an earlier run's PNG or heat map must not read as this run's
    Remove-Item -Force -ErrorAction SilentlyContinue $Capture
    Remove-Item -Force -ErrorAction SilentlyContinue `
        ([IO.Path]::ChangeExtension($Capture, ".diff.png"))
    if ($Capture.EndsWith(".bmp")) {
        Remove-Item -Force -ErrorAction SilentlyContinue `
            ([IO.Path]::ChangeExtension($Capture, ".png"))
    }
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
$watch = $Duration
if ($mustExit) {
    $watch = [Math]::Max($Duration, 60)
}
$exited = $proc.WaitForExit($watch * 1000)
if (-not $exited -and $expectedGolden -and $Capture.EndsWith(".bmp")) {
    # a slow start has not reached frame 60 yet: wait for its capture
    $waited = 0
    while (-not (Test-CaptureComplete $Capture) -and $waited -lt $captureTimeout) {
        if ($proc.WaitForExit(1000)) {
            $exited = $true
            break
        }
        $waited += 1
    }
    if ($waited -gt 0) {
        Write-Host "waited $waited s past the duration for frame 60"
    }
}
if ($exited) {
    $status = $proc.ExitCode
    if ($status -eq 77) {
        Write-Host "SKIP: the sample reported its content missing"
        Get-Content -ErrorAction SilentlyContinue $outLog, $errLog | Select-Object -Last 5 | ForEach-Object { Write-Host $_ }
        Remove-Item -Force -ErrorAction SilentlyContinue $outLog, $errLog
        exit 77
    }
    if ($status -ne 0) {
        Write-Host "FAIL: exited early with status $status"
    }
}
else {
    # still alive after the watch window: healthy, unless it had to exit
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    $proc.WaitForExit() | Out-Null
    if ($mustExit) {
        Write-Host "FAIL: did not exit within $watch s"
        $status = 1
    }
}

$log = @()
foreach ($f in @($outLog, $errLog)) {
    if (Test-Path $f) {
        $log += Get-Content $f
    }
}

# case-sensitive, so a message that merely says "exception" passes
$markers = "D3D12 ERROR|failed assertion|Assertion failed|Exception"
if ($log | Select-String -Pattern $markers -CaseSensitive) {
    Write-Host "FAIL: assertion or validation error"
    $status = 1
}

if ($status -ne 0) {
    Write-Host "--- last log lines ---"
    $log | Select-Object -Last 40 | ForEach-Object { Write-Host $_ }
    Remove-Item -Force -ErrorAction SilentlyContinue $outLog, $errLog
    exit 1
}
Remove-Item -Force -ErrorAction SilentlyContinue $outLog, $errLog

if (-not $Capture) {
    exit 0
}
if (-not (Test-Path $Capture)) {
    if ($expectedGolden) {
        Write-Host "FAIL: $($expectedGolden.FullName) exists, but frame 60 was not captured within $($Duration + $captureTimeout) s"
        exit 1
    }
    # headless samples have no swapchain, so nothing to dump
    Write-Host "note: no frame captured (sample presented no frame?)"
    exit 0
}

# uncompressed BMP is bulky
$haveTool = Test-Path $tool
if ($Capture.EndsWith(".bmp") -and $haveTool) {
    $png = [IO.Path]::ChangeExtension($Capture, ".png")
    $converted = Invoke-ImageCompare -Arguments @("--convert", $Capture, $png)
    if ($converted -eq 0) {
        Remove-Item -Force $Capture
        $Capture = $png
    }
}
Write-Host "captured frame: $Capture"

$golden = Get-ChildItem -ErrorAction SilentlyContinue -Path @(
    (Join-Path $repoRoot "Engine\*\Sample\Golden\$sampleName.$backend.png"),
    (Join-Path $repoRoot "Engine\*\Spike\Golden\$sampleName.$backend.png")
) | Select-Object -First 1
if (-not $golden) {
    exit 0
}
$golden = $golden.FullName

$captureAt = $env:CROWY_SMOKE_CAPTURE_AT
if ($captureAt -and $captureAt -ne "60") {
    Write-Host "note: not compared with $golden (captured frame $captureAt, the golden is frame 60)"
    exit 0
}
if (-not $haveTool) {
    Write-Host "FAIL: $golden exists, but $tool is not built"
    exit 1
}

$compared = Invoke-ImageCompare -Arguments @($Capture, $golden)
if ($compared -eq 0) {
    exit 0
}
if ($compared -eq 1) {
    $diff = [IO.Path]::ChangeExtension($Capture, ".diff.png")
    Write-Host "FAIL: capture differs from $golden"
    # a passing run leaves no heat map, so only a difference writes one
    $null = Invoke-ImageCompare -Arguments @($Capture, $golden, "--diff", $diff) -Quiet
    if (Test-Path $diff) {
        Write-Host "diff: $diff"
    }
    Write-Host "to accept: Copy-Item -Force '$Capture' '$golden'"
    exit 1
}
Write-Host "FAIL: could not compare against $golden"
exit 1
