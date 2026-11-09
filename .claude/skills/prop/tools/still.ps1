# Captures one still of a prop in the engine. Run from the repo root.
#
#   powershell -NoProfile -File .claude/skills/prop/tools/still.ps1 -Mesh Props/Interact/FloorLever.fbx -Materials Stone,Bronze,Iron,Glow
#       [-Az 35] [-El 20] [-Dist 3] [-Target 0,0.6,0] [-Fov 40] [-NoFloor] [-Frame 60] [-Config Debug] -Name FloorLever-r1
#
# Writes captures/prop/<Name>.json, runs Game.exe to the frame, and leaves captures/prop/<Name>.png.
param(
    [Parameter(Mandatory = $true)][string]$Mesh,
    [string]$Materials = '',
    [double]$Az = 35,
    [double]$El = 20,
    [double]$Dist = 3,
    [string]$Target = '0,0.5,0',
    [double]$Fov = 40,
    [switch]$NoFloor,
    [int]$Frame = 60,
    [string]$Config = 'Debug',
    [Parameter(Mandatory = $true)][string]$Name
)

$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$tools = Split-Path -Parent $MyInvocation.MyCommand.Path
$outDir = Join-Path $root 'captures\prop'
New-Item -ItemType Directory -Force $outDir | Out-Null

$scene = Join-Path $outDir "$Name.json"
$bmp = Join-Path $outDir "$Name.bmp"
$png = Join-Path $outDir "$Name.png"

$sceneArgs = @('-I', (Join-Path $tools 'prop_scene.py'), '--mesh', $Mesh,
    '--az', $Az, '--el', $El, '--dist', $Dist, '--target', $Target, '--fov', $Fov, '--out', $scene)
if ($Materials) { $sceneArgs += @('--materials', $Materials) }
if ($NoFloor) { $sceneArgs += '--no-floor' }
& python @sceneArgs
if ($LASTEXITCODE -ne 0) { throw "prop_scene.py failed ($LASTEXITCODE)" }

$game = Join-Path $root "build\bin\$Config\Game.exe"
$convert = Join-Path $root "build\bin\$Config\ImageCompareCheck.exe"
if (-not (Test-Path $game)) { throw "missing ${game}: build the Game target first" }

$env:CROWY_WINDOW_DISPLAY = '1'
$env:CROWY_COMMAND_PORT = '0'
if (Test-Path $bmp) { Remove-Item $bmp }
& $game (Resolve-Path -Relative $scene) --capture-frame $Frame $bmp --exit-after-frame $Frame
if ($LASTEXITCODE -ne 0) { throw "Game.exe exited with $LASTEXITCODE" }
if (-not (Test-Path $bmp)) { throw "no capture at $bmp" }

& $convert --convert $bmp $png
if ($LASTEXITCODE -ne 0) { throw "ImageCompareCheck --convert exited with $LASTEXITCODE" }
Remove-Item $bmp
Write-Output "[still] $png"
