# The input layer checked through the command port, with no hand on the
# mouse: Playground runs held with --port-input, so its devices are not heard
# and its window never takes the focus, and every frame is a counted step.
# The script pushes the events a person would make and asserts what the
# camera and read_input report; it exits 1 on the first check that fails.
#
# usage: Tools/input_checks.ps1 [-Exe build/bin/Debug/Playground.exe] [-Port 27530]
#
# Debug builds only (the port). Run from the repository root.
param(
    [string]$Exe = "build\bin\Debug\Playground.exe",
    [int]$Port = 27530
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "port.ps1")
Set-PortNumber $Port

$repoRoot = Split-Path -Parent $PSScriptRoot
$Exe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Exe)

# Playground's fly camera: radians per mouse point, meters per second
$lookSensitivity = 0.003
$moveSpeed = 5.0
# a step is one fixed tick
$stepSeconds = 1.0 / 60.0

# window points in a 1280 x 720 run with no saved layout: the scene, the
# panel P opens at 8,8 360 x 640, its title bar, and the Scene View image
# (window 376,8 640 x 400)
$scenePoint = @(900, 500)
$panelPoint = @(150, 300)
$panelTitle = @(120, 18)
$imagePoint = @(696, 220)

function Assert-That([bool]$Condition, [string]$What) {
    if (-not $Condition) {
        throw "FAIL: $What"
    }
    Write-Host "ok: $What"
}

function Test-Near([double]$Read, [double]$Want, [double]$Tolerance = 1e-4) {
    return [math]::Abs($Read - $Want) -le $Tolerance
}

# `Count` frames let through and ended
function Step-Frames([int]$Count = 1) {
    $frame = (Invoke-Port ping).frame + $Count
    $null = Invoke-Port run @{ frames = $Count }
    $null = Invoke-Port wait_frame @{ frame = $frame }
}

# the events go in at the next step
function Push-Input([object[]]$Events) {
    $null = Invoke-Port push_input @{ events = $Events }
}

# the mouse put at a point: the UI sees what is under it in the frame after,
# and the gate it sets reaches the app in the one after that
function Move-Mouse([int[]]$Point) {
    Push-Input @(@{ moveTo = $Point })
    Step-Frames 3
}

# a press and a release in frames of their own, as ImGui takes one change
# of a button per frame
function Click-Key([string]$Key) {
    Push-Input @(@{ key = $Key; down = $true })
    Step-Frames 1
    Push-Input @(@{ key = $Key; down = $false })
    Step-Frames 1
}

function Read-Yaw {
    return [double](Get-PortProperty camera.look yaw)
}

# a right-button drag by `Dx` points: press, move, release, each its own frame
function Drag-Look([int]$Dx) {
    Push-Input @(@{ button = "RButton"; down = $true })
    Step-Frames 1
    Push-Input @(@{ move = @($Dx, 0) })
    Step-Frames 1
    $during = Invoke-Port read_input
    Push-Input @(@{ button = "RButton"; down = $false })
    Step-Frames 1
    return $during
}

# meters walked with W held for `Frames` counted frames
function Walk([int]$Frames = 12) {
    $before = Get-PortProperty camera.transform position
    Push-Input @(@{ key = "W"; down = $true })
    Step-Frames $Frames
    Push-Input @(@{ key = "W"; down = $false })
    Step-Frames 1
    $after = Get-PortProperty camera.transform position
    $sum = 0.0
    for ($i = 0; $i -lt 3; ++$i) {
        $sum += [math]::Pow([double]$after[$i] - [double]$before[$i], 2)
    }
    return [math]::Sqrt($sum)
}

# Playground held, on `Port` alone, hearing only the port
function Start-Playground([string]$Debug) {
    $saved = @{}
    foreach ($name in "CROWY_COMMAND_PORT", "CROWY_DEBUG") {
        $saved[$name] = [Environment]::GetEnvironmentVariable($name)
    }
    $env:CROWY_COMMAND_PORT = "$Port"
    [Environment]::SetEnvironmentVariable("CROWY_DEBUG", $(if ($Debug) { $Debug } else { $null }))
    try {
        $process = Start-Process -FilePath $Exe -ArgumentList "--hold", "--port-input" -PassThru -WorkingDirectory $repoRoot
    }
    finally {
        foreach ($name in $saved.Keys) {
            [Environment]::SetEnvironmentVariable($name, $saved[$name])
        }
    }
    $null = $process.Handle
    $ping = Wait-Port 120
    if ($ping.app -ne "Playground") {
        throw "port $Port answers for '$($ping.app)', not Playground"
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
    $process = Start-Playground ""
    Step-Frames 2

    # 1. a look on the scene turns by the motion, with the cursor hidden
    Move-Mouse $scenePoint
    $yaw = Read-Yaw
    $during = Drag-Look 600
    $after = Invoke-Port read_input
    Assert-That (Test-Near ((Read-Yaw) - $yaw) (600 * $lookSensitivity)) "a right drag of 600 points turns the camera 1.8 rad"
    Assert-That ([bool]$during.relativeMouse) "the look hides the cursor while it turns"
    Assert-That (-not [bool]$after.relativeMouse) "letting go shows it again"
    Assert-That ((Test-Near ([double]$after.mouse.pos[0]) $scenePoint[0]) -and (Test-Near ([double]$after.mouse.pos[1]) $scenePoint[1])) "the cursor is back where the look began"

    # 2. the panel keeps the mouse it is under
    Click-Key "P"
    Move-Mouse $panelPoint
    Assert-That ([bool](Invoke-Port read_input).ui.mouse) "the panel under the mouse has it"
    $yaw = Read-Yaw
    $null = Drag-Look 300
    Assert-That (Test-Near ((Read-Yaw) - $yaw) 0) "a right drag on the panel leaves the camera"

    Move-Mouse $panelPoint
    $yaw = Read-Yaw
    Push-Input @(@{ button = "RButton"; down = $true })
    Step-Frames 1
    Push-Input @(@{ move = @(600, 0) })
    Step-Frames 1
    Push-Input @(@{ move = @(50, 0) })
    Step-Frames 1
    Push-Input @(@{ button = "RButton"; down = $false })
    Step-Frames 1
    Assert-That (Test-Near ((Read-Yaw) - $yaw) 0) "a drag pressed on the panel stays the panel's off it"

    Move-Mouse $scenePoint
    $yaw = Read-Yaw
    $null = Drag-Look -700
    Assert-That (Test-Near ((Read-Yaw) - $yaw) (-700 * $lookSensitivity)) "a drag pressed on the scene keeps turning over the panel"

    # 3. the wheel scales the walk, unless the panel is under it
    Move-Mouse $scenePoint
    $base = Walk
    Assert-That (Test-Near $base (12 * $stepSeconds * $moveSpeed) 1e-3) "W for 12 frames walks 1 m"
    Push-Input @(@{ wheel = 4 })
    Step-Frames 1
    $fast = Walk
    Assert-That (Test-Near ($fast / $base) ([math]::Pow(1.25, 4)) 1e-3) "four notches walk 1.25^4 as far"
    Move-Mouse $panelPoint
    Push-Input @(@{ wheel = -4 })
    Step-Frames 1
    Assert-That (Test-Near (Walk) $fast 1e-3) "the wheel on the panel leaves the speed"
    Move-Mouse $scenePoint
    Push-Input @(@{ wheel = -4 })
    Step-Frames 1

    # 4. a held key reads up while the UI holds an item
    Move-Mouse $panelTitle
    Push-Input @(@{ button = "LButton"; down = $true })
    Step-Frames 3
    Assert-That ([bool](Invoke-Port read_input).ui.keyboard) "holding the panel's title bar takes the keyboard"
    Assert-That (Test-Near (Walk) 0 1e-6) "W walks nowhere while it does"
    Push-Input @(@{ button = "LButton"; down = $false })
    Step-Frames 1
    Click-Key "P"

    Stop-Playground $process
    $process = $null

    # 5. the Scene View's image gives its presses to the scene
    $process = Start-Playground '{"sceneView":true}'
    Step-Frames 2
    Move-Mouse $imagePoint
    Assert-That (-not [bool](Invoke-Port read_input).ui.mouse) "the image under the mouse leaves it to the scene"
    $yaw = Read-Yaw
    $null = Drag-Look 400
    Assert-That (Test-Near ((Read-Yaw) - $yaw) (400 * $lookSensitivity)) "a right drag on the image turns the camera"
    Push-Input @(@{ button = "LButton"; down = $true })
    Step-Frames 3
    Assert-That (-not [bool](Invoke-Port read_input).ui.keyboard) "a press held on the image leaves the keyboard to the scene"
    Assert-That ((Walk) -gt 0.5) "W walks while it does"
    Push-Input @(@{ button = "LButton"; down = $false })
    Step-Frames 1
    $yaw = Read-Yaw
    $null = Drag-Look 100
    Assert-That (Test-Near ((Read-Yaw) - $yaw) (100 * $lookSensitivity)) "the image is still under the same point after the left drag"
    Click-Key "P"
    Move-Mouse $panelPoint
    $yaw = Read-Yaw
    $null = Drag-Look 300
    Assert-That (Test-Near ((Read-Yaw) - $yaw) 0) "a right drag on the panel beside it leaves the camera"
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
Write-Host "the input checks ran"
