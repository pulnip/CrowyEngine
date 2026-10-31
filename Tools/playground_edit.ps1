# Playground's Scene View over the command port, as a person drives it with
# the mouse: a click on a sphere, its transform read, a meter east and a
# quarter turn on the gizmo, the old point missing it, the sun turned on its
# ring, and no press taken once the view is off. A fresh run then draws the
# launch picture again: nothing was written back.
# Every step is a property write the mouse makes too; the script exits 1 on
# the first that fails.
#
# usage: Tools/playground_edit.ps1 [-Out captures/playground-edit] [-Exe build/bin/Debug/Playground.exe] [-Port 27540]
#
# Debug builds only (the port). Run from the repository root.
param(
    [string]$Out = "captures\playground-edit",
    [string]$Exe = "build\bin\Debug\Playground.exe",
    [int]$Port = 27540
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "port.ps1")
Set-PortNumber $Port

$repoRoot = Split-Path -Parent $PSScriptRoot
$Exe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Exe)
$tool = Join-Path (Split-Path -Parent $Exe) "ImageCompareCheck.exe"
if (-not (Test-Path $tool)) {
    throw "the run compares a capture with $tool; build ImageCompareCheck first"
}
$baseline = Join-Path $repoRoot "Engine\Render\Sample\Golden\Playground.dx12.png"
$null = New-Item -ItemType Directory -Force -Path $Out
$Out = (Resolve-Path $Out).Path

# dielectric-2-sphere's middle in the Scene View's image at launch, viewport
# points (measured on Metal)
$spherePoint = @(312, 183)
$sphere = "dielectric-2-sphere"

function Assert-That([bool]$Condition, [string]$What) {
    if (-not $Condition) {
        throw "FAIL: $What"
    }
    Write-Host "ok: $What"
}

function Test-NearAll($Read, $Want, [double]$Tolerance = 1e-5) {
    for ($i = 0; $i -lt $Want.Count; ++$i) {
        if ([math]::Abs([double]$Read[$i] - [double]$Want[$i]) -gt $Tolerance) {
            return $false
        }
    }
    return $true
}

# the Hamilton product q r, as the engine's quat(q, r)
function Get-QuatProduct($q, $r) {
    $q = @($q | ForEach-Object { [double]$_ })
    $r = @($r | ForEach-Object { [double]$_ })
    return @(
        ($q[3] * $r[0] + $q[0] * $r[3] + $q[1] * $r[2] - $q[2] * $r[1]),
        ($q[3] * $r[1] - $q[0] * $r[2] + $q[1] * $r[3] + $q[2] * $r[0]),
        ($q[3] * $r[2] + $q[0] * $r[1] - $q[1] * $r[0] + $q[2] * $r[3]),
        ($q[3] * $r[3] - $q[0] * $r[0] - $q[1] * $r[1] - $q[2] * $r[2])
    )
}

# `Count` frames let through and ended
function Step-Frames([int]$Count = 1) {
    $frame = (Invoke-Port ping).frame + $Count
    $null = Invoke-Port run @{ frames = $Count }
    $null = Invoke-Port wait_frame @{ frame = $frame }
}

# pressed at the aim's grab, let go at its reach
function Move-Handle([string]$Aim, [string]$Handle) {
    $at = Get-PortProperty gizmo $Aim
    Set-PortProperty editor grab @($at.grab)
    $held = Get-PortProperty editor handle
    Assert-That ($held -eq $Handle) "a press at the $Aim aim holds $Handle ($(Get-PortProperty editor status))"
    Set-PortProperty editor drag @($at.reach)
    Set-PortProperty editor handle "None"
}

# Playground held, on `Port` alone
function Start-Playground {
    $answers = $true
    try {
        $null = Invoke-Port ping
    }
    catch {
        $answers = $false
    }
    if ($answers) {
        throw "port $Port already answers; close what holds it first"
    }
    $saved = @{}
    foreach ($name in "CROWY_COMMAND_PORT", "CROWY_DEBUG", "CROWY_WINDOW_DISPLAY") {
        $saved[$name] = [Environment]::GetEnvironmentVariable($name)
    }
    $env:CROWY_COMMAND_PORT = "$Port"
    $env:CROWY_WINDOW_DISPLAY = "1"
    [Environment]::SetEnvironmentVariable("CROWY_DEBUG", '{"sceneView":true}')
    try {
        $process = Start-Process -FilePath $Exe -ArgumentList "--hold" -PassThru -WorkingDirectory $repoRoot
    }
    finally {
        foreach ($name in $saved.Keys) {
            [Environment]::SetEnvironmentVariable($name, $saved[$name])
        }
    }
    $null = $process.Handle
    try {
        $ping = Wait-Port 120
        if ($ping.app -ne "Playground") {
            throw "port $Port answers for '$($ping.app)', not Playground"
        }
    }
    catch {
        if (-not $process.HasExited) {
            $process.Kill()
        }
        throw
    }
    return $process
}

function Stop-Playground($Process) {
    try {
        $null = Invoke-Port quit
        $null = Invoke-Port run
    }
    catch {
    }
    if (-not $Process.WaitForExit(30000)) {
        $Process.Kill()
        throw "Playground did not quit"
    }
    if ($Process.ExitCode -ne 0) {
        throw "Playground exited $($Process.ExitCode)"
    }
}

$failed = $false
$process = $null
try {
    $process = Start-Playground
    Step-Frames 3

    # 1. a click on the sphere
    Step-Frames 1
    Set-PortProperty editor pickAt $spherePoint
    $selected = Get-PortProperty editor selected
    Assert-That ($selected -eq $sphere) "a click at $($spherePoint -join ', ') selects $sphere (got $selected)"

    # 2. its transform, a meter east on the X arrow, then a quarter turn on
    #    the Y ring, snapped
    $was = @(Get-PortProperty selection position)
    Set-PortProperty editor snap $true
    Move-Handle moveX MoveX
    $now = @(Get-PortProperty selection position)
    Assert-That (([double]$now[0] -eq [double]$was[0] + 1) -and ([double]$now[1] -eq [double]$was[1]) -and ([double]$now[2] -eq [double]$was[2])) "the arrow moved the sphere 1 m east"
    Move-Handle ring Ring
    $rotation = @(Get-PortProperty selection rotation)
    Assert-That (Test-NearAll $rotation @(0, 0.70710678, 0, 0.70710678)) "the ring turned it 90 degrees about Y ($($rotation -join ', '))"
    Set-PortProperty editor snap $false

    # 3. the rows follow at the next frame: the old point misses the sphere
    Step-Frames 2
    Set-PortProperty editor pickAt $spherePoint
    Assert-That ((Get-PortProperty editor selected) -ne $sphere) "a click at the old point misses the moved sphere"

    # 4. the sun on its ring: the selection and Playground's own light target
    #    read one rotation, a quarter turn about world Y onto the file's
    Set-PortProperty editor selected "sun"
    $sunBefore = @(Get-PortProperty light.sun.transform rotation)
    Set-PortProperty editor snap $true
    Move-Handle ring Ring
    Set-PortProperty editor snap $false
    $sunNow = @(Get-PortProperty light.sun.transform rotation)
    $sunSelected = @(Get-PortProperty selection rotation)
    $wanted = Get-QuatProduct @(0, 0.70710678, 0, 0.70710678) $sunBefore
    Assert-That ((Test-NearAll $sunNow $sunSelected 0) -and (Test-NearAll $sunNow $wanted)) "the ring turned the sun a quarter about world Y, as light.sun.transform reads it"

    # 5. nothing selected, then no view to press on
    Set-PortProperty editor selected ""
    Assert-That ((Get-PortProperty editor selected) -eq "") "an empty name clears the selection"
    Set-PortProperty debug sceneView $false
    Step-Frames 2
    Set-PortProperty editor pickAt $spherePoint
    Assert-That ((Get-PortProperty editor selected) -eq "") "a click with the view off selects nothing"
}
catch {
    Write-Host $_
    $failed = $true
}
finally {
    if ($process) {
        try {
            Stop-Playground $process
        }
        catch {
            Write-Host $_
            $failed = $true
        }
    }
}
if ($failed) {
    exit 1
}

# 6. nothing was written back: a fresh run draws the baseline
$fresh = Join-Path $Out "fresh.bmp"
$savedPort = $env:CROWY_COMMAND_PORT
$savedDisplay = $env:CROWY_WINDOW_DISPLAY
$savedDebug = [Environment]::GetEnvironmentVariable("CROWY_DEBUG")
[Environment]::SetEnvironmentVariable("CROWY_DEBUG", $null)
$env:CROWY_COMMAND_PORT = "0"
$env:CROWY_WINDOW_DISPLAY = "1"
try {
    & (Join-Path $PSScriptRoot "smoke_run.ps1") $Exe $fresh | Out-Null
    $smoked = $LASTEXITCODE
}
finally {
    $env:CROWY_COMMAND_PORT = $savedPort
    $env:CROWY_WINDOW_DISPLAY = $savedDisplay
    [Environment]::SetEnvironmentVariable("CROWY_DEBUG", $savedDebug)
}
if ($smoked -ne 0) {
    Write-Host "FAIL: the fresh run captured frame 60"
    exit 1
}
& $tool ([IO.Path]::ChangeExtension($fresh, ".png")) $baseline --tolerance 0 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL: the fresh run draws $baseline"
    exit 1
}
Write-Host "ok: a fresh run draws $baseline at tolerance 0"
Write-Host "the edit ran: $Out"
