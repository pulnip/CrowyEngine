# The owner's one task in StageEditor, performed over the command port the
# way a person performs it by hand: go to the street cut, click the street
# lamp, read it, move it a meter, switch to night, capture. Every step is a
# property write the panels make too; the script asserts each outcome and
# exits 1 on the first that fails.
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
    Assert-That ([math]::Abs($position[0] - $lampRow.x) -lt 1e-4 -and [math]::Abs($position[2] - $lampRow.z) -lt 1e-4) "its position is the scene file's ($($lampRow.x), $($lampRow.y), $($lampRow.z))"
    Assert-That ([math]::Abs($yaw - $lampRow.yaw) -lt 1e-4) "its yaw is the scene file's $($lampRow.yaw)"

    # 4. a meter east, and the picture moves with it
    $before = Save-Capture "before-move"
    # a Vec3 is one leaf: the port writes it whole
    Set-PortProperty selection position @(($position[0] + 1.0), $position[1], $position[2])
    $moved = Get-PortProperty selection position
    Assert-That ([math]::Abs($moved[0] - ($position[0] + 1.0)) -lt 1e-4) "the lamp moved 1 m east"
    $after = Save-Capture "after-move"
    & $tool $before $after | Out-Null
    Assert-That ($LASTEXITCODE -eq 1) "the capture after the move differs from the one before"

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
