# Captures StageEditor from every cut of Backlot's scene file in the chosen
# lighting keys, through the command port: a held loop, one property write
# per cut and key, one frame each. Names follow Backlot's own captures
# (<cut>.png for the default key, <cut>@<key>.png for the others), so a run
# lines up with Backlot/Captures/<run>/ for a placement comparison.
#
# usage: Tools/stage_cuts.ps1 [-Keys day,night] [-Out captures/stage/<stamp>]
#        [-Exe build/bin/StageEditor.exe] [-Backlot ../Backlot] [-Port 27520]
#
# Debug builds only (the port). Run from the repository root. The editor
# listens on its own port, so a sample already running is never driven.
param(
    [string[]]$Keys = @(),
    [string]$Out = "",
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
if (-not $Out) {
    $Out = Join-Path "captures\stage" (Get-Date -Format "yyyyMMdd-HHmmss")
}
# the editor writes the captures: it needs absolute paths, resolved where
# PowerShell stands
$Out = Resolve-LocalPath $Out
$Exe = Resolve-LocalPath $Exe
$tool = Join-Path (Split-Path -Parent $Exe) "ImageCompareCheck.exe"

$scene = Get-Content -Raw -Encoding UTF8 (Join-Path $Backlot "Data\scene.json") | ConvertFrom-Json
$cuts = @($scene.cameras | ForEach-Object { $_.name })
$defaultKey = $scene.default_key
# -File hands "day,night" over as one string
$Keys = @($Keys | ForEach-Object { $_ -split "," } | Where-Object { $_ })
if ($Keys.Count -eq 0) {
    $Keys = @($defaultKey)
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null
$process = Start-StageEditor $Exe $repoRoot $Port
try {
    # the editor's chrome stays out of the pictures
    Set-PortProperty debug showPanel $false

    foreach ($key in $Keys) {
        Set-EditorField key $key
        foreach ($cut in $cuts) {
            Set-EditorField cut $cut
            $name = if ($key -eq $defaultKey) { $cut } else { "$cut@$key" }
            # a key change rebuilds the walker on the next frame: capture the
            # one after it
            $png = Save-StageCapture (Join-Path $Out "$name.bmp") $tool ((Invoke-Port ping).frame + 2)
            Write-Host "captured $png"
        }
    }
}
finally {
    Stop-StageEditor $process
}
Write-Host "cuts: $Out"
