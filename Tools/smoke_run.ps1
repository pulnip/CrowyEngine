# Run one executable the way its ctest entry does: launch it, let it render a
# counted number of frames and exit by itself, then fail on crashes, thrown
# exceptions, or D3D12 validation errors.
#
# usage: smoke_run.ps1 <executable> [capture-image-path]
#
# The contract (frames, timeout, captures, goldens, the 77 skip) is
# docs/testing.md "Windowed runs and goldens". Run from the repository root:
# samples load Engine/Shader and Content by relative path.
param(
    [Parameter(Mandatory = $true)][string]$App,
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

if (-not $Capture -and $env:CROWY_SMOKE_CAPTURE_DIR) {
    $Capture = Join-Path $env:CROWY_SMOKE_CAPTURE_DIR "$sampleName.bmp"
}

# a spike's golden, found before the launch so the run captures its frame
$golden = Get-ChildItem -ErrorAction SilentlyContinue `
    -Path (Join-Path $repoRoot "Engine\*\Spike\Golden\$sampleName.$backend.png") |
    Select-Object -First 1
$expectedGolden = $null
if (-not $env:CROWY_SMOKE_CAPTURE_AT -or $env:CROWY_SMOKE_CAPTURE_AT -eq "60") {
    $expectedGolden = $golden
}
# a golden gates every run, not only runs that collect captures
if (-not $Capture -and $expectedGolden) {
    $Capture = Join-Path $env:TEMP "crowy-smoke\$sampleName.bmp"
}
$mustExit = $env:CROWY_SMOKE_MUST_EXIT -eq "1"
$captureAt = 60
if ($env:CROWY_SMOKE_CAPTURE_AT) {
    $captureAt = [int]$env:CROWY_SMOKE_CAPTURE_AT
}
$frames = 60
if ($env:CROWY_SMOKE_FRAMES) {
    $frames = [int]$env:CROWY_SMOKE_FRAMES
}
# a capture asks for its frame, so the run reaches it
if ($Capture -and $captureAt -gt $frames) {
    $frames = $captureAt
}
$timeout = 60
if ($env:CROWY_SMOKE_TIMEOUT) {
    $timeout = [int]$env:CROWY_SMOKE_TIMEOUT
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
}

# a check is headless and may parse arguments of its own
$launch = @{}
if (-not $mustExit) {
    $arguments = @("--exit-after-frame", "$frames")
    if ($Capture) {
        $arguments += @("--capture-frame", "$captureAt", "`"$Capture`"")
    }
    $launch.ArgumentList = $arguments
}

$proc = Start-Process -FilePath $App @launch `
    -WorkingDirectory (Get-Location) `
    -RedirectStandardOutput $outLog `
    -RedirectStandardError $errLog `
    -NoNewWindow -PassThru

# cache the process handle right away: without this, ExitCode reads back
# null when the process exits before we query it (fast headless samples)
$null = $proc.Handle

$status = 0
$exited = $proc.WaitForExit($timeout * 1000)
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
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    $proc.WaitForExit() | Out-Null
    if ($mustExit) {
        Write-Host "FAIL: did not exit within $timeout s"
    }
    else {
        Write-Host "FAIL: did not exit after frame $frames within $timeout s"
    }
    $status = 1
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
        Write-Host "FAIL: $($expectedGolden.FullName) exists, but frame 60 was not captured"
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

if (-not $golden) {
    exit 0
}
$golden = $golden.FullName

if ($captureAt -ne 60) {
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
