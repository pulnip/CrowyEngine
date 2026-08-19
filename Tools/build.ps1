<#
.SYNOPSIS
    Builds CrowyEngine exactly the way the VSCode CMake Tools extension does.

.DESCRIPTION
    VSCode's CMake Tools runs every build inside the Visual Studio Developer
    environment (see .vscode/settings.json -> "cmake.useVsDeveloperEnvironment":
    "always") and drives it through the CMake presets. A plain shell has no
    INCLUDE/LIB/VCINSTALLDIR, so builds started from one behave differently.
    This script reproduces that environment and command line, writes all output
    to log files, and never uses a pipe (pipes buffer and make a live build look
    hung).

    -Detach re-launches the script as a process that outlives its caller, so a
    long build cannot be killed by a shell timeout. Poll it with -Status.

.EXAMPLE
    Tools/build.ps1                                  # foreground, configure if needed
    Tools/build.ps1 -Target CrowyEditor -Detach      # start and return immediately
    Tools/build.ps1 -Status                          # running? / exit code / log tail
#>
[CmdletBinding()]
param(
    [string]   $Preset = 'Windows-llvm-x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release')]
    [string]   $Config = 'Debug',
    [string[]] $Target,
    [switch]   $Clean,
    [switch]   $Fresh,
    [switch]   $Detach,
    [switch]   $Status,
    [string]   $LogFile
)

$ErrorActionPreference = 'Stop'
$repoRoot  = Split-Path -Parent $PSScriptRoot
$binaryDir = Join-Path $repoRoot 'build'

if (-not $LogFile) { $LogFile = Join-Path $binaryDir 'build-log.txt' }
$configureLog = Join-Path $binaryDir 'configure-log.txt'
$summaryFile  = Join-Path $binaryDir 'build-summary.txt'
$pidFile      = Join-Path $binaryDir 'build.pid'
$statusFile   = Join-Path $binaryDir 'build-status.txt'

New-Item -ItemType Directory -Force -Path $binaryDir | Out-Null

# ------------------------------------------------------------------- -Status
if ($Status) {
    $running = $false
    if (Test-Path $pidFile) {
        $bpid = (Get-Content $pidFile -Raw).Trim()
        $running = [bool](Get-Process -Id $bpid -ErrorAction SilentlyContinue)
    }
    Write-Output "running : $running"
    if (Test-Path $statusFile) { Write-Output "status  : $((Get-Content $statusFile -Raw).Trim())" }

    $live = if (Test-Path $LogFile) { $LogFile } elseif (Test-Path $configureLog) { $configureLog } else { $null }
    if ($live) {
        Write-Output "log     : $live  (updated $((Get-Item $live).LastWriteTime))"
        Write-Output '--- tail ---'
        Get-Content $live -Tail 15 | ForEach-Object { Write-Output $_ }
    }
    if ((-not $running) -and (Test-Path $summaryFile)) {
        Write-Output '--- summary ---'
        Get-Content $summaryFile | ForEach-Object { Write-Output $_ }
    }
    exit 0
}

# ------------------------------------------------------------------- -Detach
# Start a copy of this script that survives the caller. Windows does not kill
# a child when the parent exits, so the build keeps going across tool timeouts
# and session teardown.
if ($Detach) {
    if (Test-Path $pidFile) {
        $bpid = (Get-Content $pidFile -Raw).Trim()
        if (Get-Process -Id $bpid -ErrorAction SilentlyContinue) {
            Write-Output "A build is already running (PID $bpid). Exactly one build at a time - wait for it."
            exit 1
        }
    }
    Remove-Item $statusFile -Force -ErrorAction SilentlyContinue

    $relaunch = @('-NoProfile', '-File', $PSCommandPath, '-Preset', $Preset, '-Config', $Config)
    if ($Target) { $relaunch += '-Target'; $relaunch += ($Target -join ',') }
    if ($Clean)  { $relaunch += '-Clean' }
    if ($Fresh)  { $relaunch += '-Fresh' }

    $child = Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList $relaunch `
        -WorkingDirectory $repoRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $summaryFile -RedirectStandardError "$summaryFile.err"

    $child.Id | Set-Content $pidFile
    Write-Output "detached build started (PID $($child.Id))"
    Write-Output "poll with : Tools/build.ps1 -Status"
    exit 0
}

# ---------------------------------------------------------------- VS dev env
# Locate the VS install the same way CMake Tools does, then import the
# environment VsDevCmd.bat sets up into this process.
function Import-VsDevEnv {
    param([string] $Arch = 'x64', [string] $HostArch = 'x64')

    if ($env:VCINSTALLDIR) { return }   # already inside a developer shell

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at $vswhere - is Visual Studio installed?"
    }

    $vsRoot = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsRoot) { throw 'No Visual Studio install with the C++ toolset was found.' }

    $vsDevCmd = Join-Path $vsRoot 'Common7/Tools/VsDevCmd.bat'
    if (-not (Test-Path $vsDevCmd)) { throw "VsDevCmd.bat not found at $vsDevCmd" }

    # `set` after the batch file runs = the exact environment CMake Tools uses.
    $dump = cmd /c "`"$vsDevCmd`" -arch=$Arch -host_arch=$HostArch -no_logo >nul 2>&1 && set"
    foreach ($line in $dump) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path ("Env:" + $Matches[1]) -Value $Matches[2] -ErrorAction SilentlyContinue
        }
    }
    if (-not $env:VCINSTALLDIR) { throw 'Failed to import the VS developer environment.' }
}

Import-VsDevEnv

# ------------------------------------------------------------------- command
# Runs cmake with output redirected (never piped) into $Log, then folds stderr
# into the same file. Returns the exit code and nothing else — any Write-Output
# in here would be folded into the return value and break the caller's `-ne 0`
# check, so progress messages use Write-Host.
function Invoke-CMake {
    param([string[]] $CMakeArgs, [string] $Log)

    Write-Host "command : cmake $($CMakeArgs -join ' ')"
    Write-Host "log     : $Log"

    $p = Start-Process -FilePath 'cmake' -ArgumentList $CMakeArgs `
        -WorkingDirectory $repoRoot -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $Log -RedirectStandardError "$Log.err"

    if (Test-Path "$Log.err") {
        Get-Content "$Log.err" | Add-Content $Log
        Remove-Item "$Log.err" -Force
    }
    return [int] $p.ExitCode
}

Write-Output "repo    : $repoRoot"
Write-Output ''
$sw = [System.Diagnostics.Stopwatch]::StartNew()

# Configure when the cache is missing (fresh clone, or build/ was deleted).
# CMake Tools does this implicitly; a bare `cmake --build --preset` would fail
# with "could not load cache".
if ($Fresh -or -not (Test-Path (Join-Path $binaryDir 'CMakeCache.txt'))) {
    $configureArgs = @('--preset', $Preset)
    if ($Fresh) { $configureArgs += '--fresh' }

    Write-Output '=== configure ==='
    $code = Invoke-CMake -CMakeArgs $configureArgs -Log $configureLog
    Write-Output "configure exit code : $code  ($([math]::Round($sw.Elapsed.TotalSeconds, 1)) s)"
    Write-Output ''
    if ($code -ne 0) {
        Write-Output '--- configure log tail ---'
        Get-Content $configureLog -Tail 40 | ForEach-Object { Write-Output $_ }
        "configure FAILED ($code)" | Set-Content $statusFile
        exit $code
    }
}

# Build preset names are "<configure-preset>-<Config>"; passing a configure
# preset where a build preset belongs fails with "No such build preset".
$buildPreset = "$Preset-$Config"

$cmakeArgs = @('--build', '--preset', $buildPreset)
if ($Clean)  { $cmakeArgs += '--clean-first' }
if ($Target) { $cmakeArgs += @('--target') + $Target }

Write-Output '=== build ==='
$exitCode = Invoke-CMake -CMakeArgs $cmakeArgs -Log $LogFile
$sw.Stop()

# 'error:' / 'FAILED:' with the colon - a bare 'error' also matches source file
# names like SDL_error.c.obj scrolling past in the log.
$errors = @(Select-String -Path $LogFile -Pattern 'error:', 'FAILED:' -SimpleMatch -ErrorAction SilentlyContinue)

Write-Output ''
Write-Output "exit code : $exitCode"
Write-Output "elapsed   : $([math]::Round($sw.Elapsed.TotalSeconds, 1)) s (configure + build)"
Write-Output "error/FAILED lines in log : $($errors.Count)"
if ($errors.Count -gt 0) {
    Write-Output '--- first 40 ---'
    $errors | Select-Object -First 40 | ForEach-Object { Write-Output $_.Line }
}

"exit $exitCode after $([math]::Round($sw.Elapsed.TotalSeconds, 1)) s" | Set-Content $statusFile
exit $exitCode
