# The owner's one task in StageEditor, performed over the command port the
# way a person performs it by hand: go to the street cut, click the street
# lamp, read it, drag it a meter and turn it a quarter on the gizmo, switch
# to night, capture. Every step is a property write the panels and the mouse
# make too; the script asserts each outcome and exits 1 on the first that fails.
#
# usage: Tools/stage_task.ps1 [-Out captures/stage-task] [-Exe build/bin/StageEditor.exe]
#        [-Backlot ../Backlot] [-Port 27520]
#
# Debug builds only (the port). Run from the repository root.
param(
    [string]$Out = "captures\stage-task",
    [string]$Exe = "build\bin\StageEditor.exe",
    [string]$Backlot = "",
    [int]$Port = 27520
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "port.ps1")
. (Join-Path $PSScriptRoot "stage_editor.ps1")
Set-PortNumber $Port

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $Backlot) {
    $Backlot = Join-Path (Split-Path -Parent $repoRoot) "Backlot"
}
$Out = Resolve-LocalPath $Out
$Exe = Resolve-LocalPath $Exe
$tool = Join-Path (Split-Path -Parent $Exe) "ImageCompareCheck.exe"
if (-not (Test-Path $tool)) {
    throw "the task compares captures with $tool; build ImageCompareCheck first"
}

# lamp-ne's pole in the street cut at 1920 x 1080 (measured against Backlot a6179ef)
$lampPixel = @(1076, 450)
$lampName = "instance/lamp-ne"

$scene = Get-Content -Raw -Encoding UTF8 (Join-Path $Backlot "Data\scene.json") | ConvertFrom-Json
$lampRow = $scene.instances | Where-Object { $_.name -eq "lamp-ne" }

function Assert-That([bool]$Condition, [string]$What) {
    if (-not $Condition) {
        throw "FAIL: $What"
    }
    Write-Host "ok: $What"
}

# a write lands on the next frame: capture the one after it
function Save-Capture([string]$Name) {
    return Save-StageCapture (Join-Path $Out "$Name.bmp") $tool ((Invoke-Port ping).frame + 2)
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null
$process = Start-StageEditor $Exe $repoRoot $Port
$failed = $false
try {
    Set-PortProperty debug showPanel $false

    # 1. the street cut
    Set-EditorField cut street
    Assert-That $true "the street cut"
    $null = Invoke-Port run @{ frames = 3 }

    # 2. a click on the lamp's pole
    Set-PortProperty editor pickAt $lampPixel
    $selected = Get-PortProperty editor selected
    Assert-That ($selected -eq $lampName) "a click at $($lampPixel -join ', ') selects $lampName (got '$selected')"

    # 3. its name and transform, as the scene file has them
    $position = Get-PortProperty selection position
    $yaw = Get-PortProperty selection yaw
    $scale = Get-PortProperty selection scale
    $file = @($lampRow.x, $lampRow.y, $lampRow.z)
    $fileScale = @($lampRow.sx, $lampRow.sy, $lampRow.sz)
    $same = $true
    for ($axis = 0; $axis -lt 3; ++$axis) {
        $same = $same -and [math]::Abs($position[$axis] - $file[$axis]) -lt 1e-4 -and [math]::Abs($scale[$axis] - $fileScale[$axis]) -lt 1e-4
    }
    Assert-That $same "its position ($($file -join ', ')) and scale ($($fileScale -join ', ')) are the scene file's"
    Assert-That ([math]::Abs($yaw - $lampRow.yaw) -lt 1e-4) "its yaw is the scene file's $($lampRow.yaw)"

    # 4. with the gizmo, pressed and dragged where a person sees its handles:
    #    a meter east on the X arrow, then a quarter turn on the ring, snapped
    $before = Save-Capture "before-move"
    Set-PortProperty debug showPanel $true
    $null = Invoke-Port run @{ frames = 1 }
    Set-PortProperty editor snap $true
    $aim = Get-PortProperty gizmo moveX
    Set-PortProperty editor grab $aim.grab
    Assert-That ((Get-PortProperty editor handle) -eq "MoveX") "a press at the X arrow's tip ($($aim.grab -join ', ')) holds it"
    Set-PortProperty editor drag $aim.reach
    Set-PortProperty editor handle None
    $moved = Get-PortProperty selection position
    Assert-That ([math]::Abs($moved[0] - ($position[0] + 1.0)) -lt 1e-4 -and $moved[1] -eq $position[1] -and $moved[2] -eq $position[2]) "the arrow moved the lamp 1 m east to x $($moved[0])"

    $ring = Get-PortProperty gizmo ring
    Set-PortProperty editor grab $ring.grab
    Assert-That ((Get-PortProperty editor handle) -eq "Ring") "a press on the ring ($($ring.grab -join ', ')) holds it"
    Set-PortProperty editor drag $ring.reach
    Set-PortProperty editor handle None
    $turned = Get-PortProperty selection yaw
    $expected = ($lampRow.yaw + 90) % 360
    Assert-That ([math]::Abs($turned - $expected) -lt 1e-3) "the ring turned it 90 degrees to yaw $turned"
    Set-PortProperty editor snap $false
    Set-PortProperty debug showPanel $false

    $after = Save-Capture "after-move"
    & $tool $before $after | Out-Null
    Assert-That ($LASTEXITCODE -eq 1) "the capture after the move differs from the one before"

    # the pick follows the move: the old pixel misses the lamp, and a scan
    # along its row finds the pole again
    Set-PortProperty editor pickAt $lampPixel
    $old = Get-PortProperty editor selected
    Assert-That ($old -ne $lampName) "a click at the old pixel no longer selects the lamp (got '$old')"
    $found = -1
    for ($offset = 3; $offset -le 900 -and $found -lt 0; $offset += 3) {
        foreach ($x in @(($lampPixel[0] + $offset), ($lampPixel[0] - $offset))) {
            if ($x -lt 0 -or $x -ge 1920) {
                continue
            }
            Set-PortProperty editor pickAt @($x, $lampPixel[1])
            if ((Get-PortProperty editor selected) -eq $lampName) {
                $found = $x
                break
            }
        }
    }
    Assert-That ($found -ge 0) "a click at $found, $($lampPixel[1]) selects the moved lamp"

    # 5. night, and a capture
    Set-EditorField key night
    Assert-That $true "the night key"
    $night = Save-Capture "night"
    Assert-That (Test-Path $night) "captured $night"
}
catch {
    Write-Host $_
    $failed = $true
}
finally {
    Stop-StageEditor $process
}
if ($failed) {
    exit 1
}
Write-Host "the task ran: $Out"
