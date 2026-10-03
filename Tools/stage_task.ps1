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

# the read vector equal to the wanted one, component by component
function Test-Vector($Read, [double[]]$Want) {
    if (@($Read).Count -ne $Want.Count) {
        return $false
    }
    for ($i = 0; $i -lt $Want.Count; ++$i) {
        if ([math]::Abs([double]$Read[$i] - $Want[$i]) -gt 1e-6) {
            return $false
        }
    }
    return $true
}

# the two pictures equal at tolerance 0
function Test-SamePicture([string]$A, [string]$B) {
    & $tool $A $B --tolerance 0 | Out-Null
    return $LASTEXITCODE -eq 0
}

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

    # 0. the launch: the first revision of Backlot's own scene file
    $sceneFile = Join-Path $Backlot "Data\scene.json"
    $original = Get-PortProperty editor scene
    Assert-That ((Get-PortProperty editor revision) -eq 1) "revision 1 at launch"
    Assert-That ((Resolve-Path $original).Path -eq (Resolve-Path $sceneFile).Path) "the scene file is Backlot's ($original)"
    Assert-That ((Get-PortProperty editor paused) -eq $true -and (Get-PortProperty editor time) -eq 0) "scene time is paused at 0"

    # 1. the street cut
    Set-EditorField cut street
    Assert-That $true "the street cut"
    $null = Invoke-Port run @{ frames = 3 }

    # 1b. reading the same file again redraws the same picture
    Set-EditorField key night
    $nightBefore = Save-Capture "night-before"
    Set-PortProperty editor reload $true
    Assert-That ((Get-PortProperty editor reload) -eq $true) "a reload waits for the next frame"
    $nightAgain = Save-Capture "night-reloaded-same"
    Assert-That ((Get-PortProperty editor revision) -eq 2) "the reload read the file: $(Get-PortProperty editor status)"
    Assert-That (Test-SamePicture $nightBefore $nightAgain) "a reload of the same file redraws the same picture at tolerance 0"
    Set-EditorField key day

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

    # 6. a scratch copy of the scene file with the lamp where the gizmo put
    #    it: reloaded, it draws the gizmo's picture exactly
    $sceneText = [IO.File]::ReadAllText($sceneFile)
    $lampLine = '"name": "lamp-ne", "area": "Street", "model": "StreetLamp", "x": 10.5, "y": 0.15, "z": 7.0, "yaw": 180.0,'
    $movedLine = '"name": "lamp-ne", "area": "Street", "model": "StreetLamp", "x": 11.5, "y": 0.15, "z": 7.0, "yaw": 270.0,'
    Assert-That ($sceneText.Contains($lampLine)) "the scene file holds lamp-ne's row"
    $utf8 = New-Object Text.UTF8Encoding $false
    $movedFile = Join-Path $Out "scene-moved.json"
    [IO.File]::WriteAllText($movedFile, $sceneText.Replace($lampLine, $movedLine), $utf8)
    Set-PortProperty editor scene $movedFile
    Set-PortProperty editor reload $true
    $reloaded = Save-Capture "night-reloaded-moved"
    Assert-That ((Get-PortProperty editor revision) -eq 3) "the scratch copy loaded: $(Get-PortProperty editor status)"
    Assert-That ((Get-PortProperty editor selected) -eq $lampName) "the selection is kept by name"
    $position = Get-PortProperty selection position
    Assert-That ($position[0] -eq 11.5 -and (Get-PortProperty selection yaw) -eq 270) "the file puts the lamp at x 11.5, yaw 270"
    Assert-That (Test-SamePicture $night $reloaded) "the reloaded copy draws the gizmo's picture at tolerance 0"

    # 7. a file naming geometry the launch did not load is refused whole
    $refusedFile = Join-Path $Out "scene-refused.json"
    $refusedText = $sceneText.Replace($lampLine, $movedLine).Replace("Models/Street/StreetLamp.fbx", "Models/Street/StreetLampMoved.fbx")
    [IO.File]::WriteAllText($refusedFile, $refusedText, $utf8)
    Set-PortProperty editor scene $refusedFile
    Set-PortProperty editor reload $true
    $refused = Save-Capture "night-refused"
    $status = Get-PortProperty editor status
    Assert-That ((Get-PortProperty editor revision) -eq 3 -and $status.Contains("restart")) "a model the launch did not load is refused: $status"
    Assert-That ((Get-PortProperty editor scene) -eq $movedFile) "the scene file goes back to the one the rows came from"
    Assert-That (Test-SamePicture $reloaded $refused) "a refused reload changes nothing on screen"

    # 8. Backlot's own file again: the file wins over the gizmo
    Set-PortProperty editor scene $original
    Set-PortProperty editor reload $true
    $restored = Save-Capture "night-restored"
    Assert-That ((Get-PortProperty editor revision) -eq 4) "the original file loaded again"
    $position = Get-PortProperty selection position
    Assert-That ($position[0] -eq 10.5 -and (Get-PortProperty selection yaw) -eq 180) "the lamp is back at x 10.5, yaw 180"
    Assert-That (Test-SamePicture $nightBefore $restored) "the original file redraws the launch's picture at tolerance 0"

    # 9. scene time: a seek shows its frame, a counted run plays the loop's
    #    step, and the same play twice draws the same picture
    Set-PortProperty editor selected quad/screen-ne
    Set-PortProperty editor time 0.32
    $rect = Get-PortProperty selection.material uvScaleOffset
    Assert-That (Test-Vector $rect @(0.5, 0.25, 0, 0.25)) "at 0.32 s screen-ne shows frame 2 ($($rect -join ', '))"
    $frame = (Invoke-Port ping).frame
    Set-PortProperty editor paused $false
    $null = Invoke-Port run @{ frames = 30 }
    $null = Invoke-Port wait_frame @{ frame = $frame + 30 }
    Set-PortProperty editor paused $true
    $time = Get-PortProperty editor time
    $rect = Get-PortProperty selection.material uvScaleOffset
    Assert-That ([math]::Abs($time - 0.82) -lt 1e-4 -and (Test-Vector $rect @(0.5, 0.25, 0.5, 0.5))) "30 counted frames play to $time s, frame 5"

    $plays = @()
    foreach ($take in 1, 2) {
        Set-PortProperty editor time 0.32
        Set-PortProperty editor paused $false
        $plays += Save-StageCapture (Join-Path $Out "play-$take.bmp") $tool ((Invoke-Port ping).frame + 30)
        Set-PortProperty editor paused $true
    }
    Assert-That (Test-SamePicture $plays[0] $plays[1]) "the same play twice draws the same picture at tolerance 0"
    Set-PortProperty editor time 0
    $still = Save-Capture "time-zero"
    Assert-That (-not (Test-SamePicture $still $plays[0])) "the played picture shows another frame than time 0"
    $rect = Get-PortProperty selection.material uvScaleOffset
    Assert-That (Test-Vector $rect @(0.5, 0.25, 0, 0)) "time 0 shows the first frame again"
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
