# Captures StageEditor from every cut of Backlot's scene file in the chosen
# lighting keys, through the command port: a held loop, one property write
# per cut and key, one frame each. Names follow Backlot's own captures
# (<cut>.png for the default key, <cut>@<key>.png for the others), so a run
# lines up with Backlot/Captures/<run>/ for a placement comparison.
#
# usage: Tools/stage_cuts.ps1 [-Keys day,night] [-Out captures/stage/<stamp>]
#        [-Exe build/bin/StageEditor.exe] [-Backlot ../Backlot]
#
# Debug builds only (the port). Run from the repository root.
param(
    [string[]]$Keys = @(),
    [string]$Out = "",
    [string]$Exe = "build\bin\StageEditor.exe",
    [string]$Backlot = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "port.ps1")

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $Backlot) {
    $Backlot = Join-Path (Split-Path -Parent $repoRoot) "Backlot"
}
if (-not $Out) {
    $Out = Join-Path "captures\stage" (Get-Date -Format "yyyyMMdd-HHmmss")
}
$tool = Join-Path (Split-Path -Parent $Exe) "ImageCompareCheck.exe"

$scene = Get-Content -Raw -Encoding UTF8 (Join-Path $Backlot "Data\scene.json") | ConvertFrom-Json
$cuts = @($scene.cameras | ForEach-Object { $_.name })
$defaultKey = $scene.default_key
# -File hands "day,night" over as one string
$Keys = @($Keys | ForEach-Object { $_ -split "," } | Where-Object { $_ })
if ($Keys.Count -eq 0) {
    $Keys = @($defaultKey)
}

# the editor reverts a write it cannot apply; read it back
function Set-EditorField([string]$Field, [string]$Value) {
    Set-PortProperty editor $Field $Value
    $now = Get-PortProperty editor $Field
    if ($now -ne $Value) {
        throw "editor.$Field stayed '$now': $(Get-PortProperty editor status)"
    }
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null
if (Get-Process -Name StageEditor -ErrorAction SilentlyContinue) {
    throw "a StageEditor is already running; the port would answer for it"
}
$process = Start-Process -FilePath $Exe -ArgumentList "--hold" -PassThru -WorkingDirectory $repoRoot
try {
    $null = Wait-Port 120
    # the editor's chrome stays out of the pictures
    Set-PortProperty debug showPanel $false

    foreach ($key in $Keys) {
        Set-EditorField key $key
        foreach ($cut in $cuts) {
            Set-EditorField cut $cut
            $name = if ($key -eq $defaultKey) { $cut } else { "$cut@$key" }
            $bmp = [IO.Path]::GetFullPath((Join-Path $Out "$name.bmp"))
            # a key change rebuilds the walker on the next frame: capture the
            # one after it
            Save-PortFrame $bmp ((Invoke-Port ping).frame + 2)
            if (Test-Path $tool) {
                $png = [IO.Path]::ChangeExtension($bmp, ".png")
                & $tool --convert $bmp $png | Out-Null
                if ($LASTEXITCODE -eq 0) {
                    Remove-Item $bmp
                }
            }
            Write-Host "captured $name"
        }
    }
}
finally {
    try {
        $null = Invoke-Port quit
    }
    catch {
    }
    if (-not $process.WaitForExit(20000)) {
        Stop-Process -Id $process.Id -Force
    }
}
Write-Host "cuts: $Out"
