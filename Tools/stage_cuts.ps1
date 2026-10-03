# Captures StageEditor from every cut of Backlot's scene file in the chosen
# lighting keys, through the command port, and compares each picture with
# its golden: a held loop, one property write per cut and key, one frame
# each. Names follow Backlot's own captures (<cut>.png for the default key,
# <cut>@<key>.png for the others), so a run lines up with
# Backlot/Captures/<run>/ for a placement comparison.
#
# The goldens are Engine/Render/Sample/Golden/StageEditor/<name>.dx12.png,
# taken at the Backlot commit StageEditor.backlot.txt names, for the keys it
# pins; the launch picture's golden is the smoke's StageEditor.dx12.png. A
# Backlot at another commit, or with changes the editor reads, is captured
# but not compared (exit 77). -Record copies the pictures that are missing
# or differ and stamps Backlot's commit.
#
# usage: Tools/stage_cuts.ps1 [-Keys day,night] [-Cuts street,roof]
#        [-Out captures/stage/<stamp>] [-Exe build/bin/StageEditor.exe]
#        [-Port 27520] [-Tolerance N] [-Record]
#
# Debug builds only (the port). Run from the repository root. The editor
# listens on its own port, so a sample already running is never driven.
param(
    [string[]]$Keys = @(),
    [string[]]$Cuts = @(),
    [string]$Out = "",
    [string]$Exe = "build\bin\StageEditor.exe",
    [int]$Port = 27520,
    [int]$Tolerance = -1,
    [switch]$Record
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "port.ps1")
. (Join-Path $PSScriptRoot "stage_editor.ps1")
Set-PortNumber $Port

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $Out) {
    $Out = Join-Path "captures\stage" (Get-Date -Format "yyyyMMdd-HHmmss")
}
# the editor writes the captures: it needs absolute paths, resolved where
# PowerShell stands
$Out = Resolve-LocalPath $Out
$Exe = Resolve-LocalPath $Exe
$tool = Join-Path (Split-Path -Parent $Exe) "ImageCompareCheck.exe"
if (-not (Test-Path $tool)) {
    throw "stage_cuts converts and compares with $tool; build ImageCompareCheck first"
}
$backend = "dx12"
$goldenRoot = Join-Path $repoRoot "Engine\Render\Sample\Golden"
$stampFile = Join-Path $goldenRoot "StageEditor.backlot.txt"
$stamp = Read-StageStamp $stampFile
# -File hands "day,night" over as one string
$Keys = @($Keys | ForEach-Object { $_ -split "," } | Where-Object { $_ })
$Cuts = @($Cuts | ForEach-Object { $_ -split "," } | Where-Object { $_ })
$compareArguments = @()
if ($Tolerance -ge 0) {
    $compareArguments = @("--tolerance", "$Tolerance")
}

# the launch picture is the smoke's golden; every other one has its own
function Get-GoldenPath([string]$Name) {
    if ($Name -eq $launchName) {
        return Join-Path $goldenRoot "StageEditor.$backend.png"
    }
    return Join-Path $goldenRoot "StageEditor\$Name.$backend.png"
}

function Get-ShortHash([string]$Hash) {
    return $Hash.Substring(0, [math]::Min(7, $Hash.Length))
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null
$exitCode = 0
$process = Start-StageEditor $Exe $repoRoot $Port
try {
    # the editor's chrome stays out of the pictures
    Set-PortProperty debug showPanel $false

    # a golden is the launch's still scene: the file it read, at time 0
    $paused = Get-PortProperty editor paused
    $time = Get-PortProperty editor time
    $revision = Get-PortProperty editor revision
    if (-not ($paused -eq $true -and $time -eq 0 -and $revision -eq 1)) {
        throw "a golden needs the launch's still scene (paused $paused, time $time, revision $revision)"
    }
    # the cut the editor launches on, in the default key: the smoke's picture
    $launchName = Get-PortProperty editor cut

    $root = Get-StageRoot (Get-PortProperty editor scene)
    $state = Get-BacklotState $root
    $scene = Get-Content -Raw -Encoding UTF8 (Join-Path $root "Data\scene.json") | ConvertFrom-Json
    $sceneCuts = @($scene.cameras | ForEach-Object { $_.name })
    $defaultKey = $scene.default_key
    foreach ($cut in $Cuts) {
        if ($sceneCuts -notcontains $cut) {
            throw "no cut '$cut' in $root (cuts: $($sceneCuts -join ', '))"
        }
    }
    if ($Cuts.Count -eq 0) {
        $Cuts = $sceneCuts
    }
    if ($Keys.Count -eq 0) {
        $Keys = if ($stamp -and $stamp.Keys.Count -gt 0) { $stamp.Keys } else { @($defaultKey) }
    }

    $head = if ($state.Head) { Get-ShortHash $state.Head } else { "an unknown commit" }
    $clean = if ($state.Changes.Count -gt 0) { "with changes" } else { "clean" }
    Write-Host "backlot: $root at $head, $clean"
    $skip = ""
    if ($Record) {
        if (-not $state.Head) {
            throw "cannot read Backlot's commit at $root, so goldens cannot be stamped"
        }
        if ($state.Changes.Count -gt 0) {
            throw "cannot stamp goldens from a Backlot with uncommitted changes the editor reads:`n  $($state.Changes -join "`n  ")"
        }
        $pinned = $Keys
    }
    else {
        $pinned = if ($stamp) { $stamp.Keys } else { @() }
        if (-not $stamp) {
            $skip = "unstamped: $stampFile does not exist"
        }
        elseif (-not $state.Head) {
            $skip = "cannot read Backlot's commit at $root"
        }
        elseif ($state.Head -ne $stamp.Backlot) {
            $skip = "Backlot is at $head, the goldens were taken at $(Get-ShortHash $stamp.Backlot)"
        }
        elseif ($state.Changes.Count -gt 0) {
            $skip = "Backlot has uncommitted changes the editor reads ($($state.Changes -join '; '))"
        }
    }

    $captured = 0
    $compared = 0
    $same = 0
    $different = 0
    $missing = 0
    $recorded = 0
    foreach ($key in $Keys) {
        Set-EditorField key $key
        foreach ($cut in $Cuts) {
            Set-EditorField cut $cut
            $name = if ($key -eq $defaultKey) { $cut } else { "$cut@$key" }
            foreach ($stale in @("$name.png", "$name.bmp", "$name.diff.png")) {
                Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $Out $stale)
            }
            # a key change rebuilds the walker on the next frame: capture the
            # one after it
            $png = Save-StageCapture (Join-Path $Out "$name.bmp") $tool ((Invoke-Port ping).frame + 2)
            ++$captured
            if ($skip) {
                Write-Host "captured  $name"
                continue
            }
            if ($pinned -notcontains $key) {
                Write-Host "captured  $name (capture only: $key is not pinned)"
                continue
            }

            $golden = Get-GoldenPath $name
            $smoke = $name -eq $launchName
            if (-not (Test-Path $golden)) {
                if ($Record -and -not $smoke) {
                    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $golden) | Out-Null
                    Copy-Item -Force $png $golden
                    ++$recorded
                    Write-Host "recorded  $name"
                    continue
                }
                ++$missing
                Write-Host "FAIL: no golden for $name on $backend ($golden)"
                continue
            }

            ++$compared
            $result = Invoke-ImageCompareCheck $tool (@($png, $golden) + $compareArguments) -Quiet
            if ($result -eq 0) {
                ++$same
                Write-Host "same      $name"
                continue
            }
            if ($result -eq 1 -and $Record -and -not $smoke) {
                Copy-Item -Force $png $golden
                ++$recorded
                Write-Host "recorded  $name (it differed)"
                continue
            }
            ++$different
            if ($result -ne 1) {
                Write-Host "FAIL: could not compare $name against $golden"
                continue
            }
            Write-Host "FAIL: $name differs from $golden"
            $diff = Join-Path $Out "$name.diff.png"
            $null = Invoke-ImageCompareCheck $tool (@($png, $golden) + $compareArguments + @("--diff", $diff))
            Write-Host "diff: $diff"
            if ($smoke) {
                Write-Host "this is the smoke's golden: if StageEditorSmoke passes, the port's held frames and the free-running frame 60 draw different pictures; accept through Tools/smoke_run.ps1 build\bin\StageEditor.exe"
            }
            else {
                Write-Host "to accept: Copy-Item -Force '$png' '$golden'"
            }
        }
    }

    if ((Get-PortProperty editor time) -ne 0) {
        throw "scene time moved during the run"
    }

    if ($Record) {
        $restamped = (-not $stamp) -or ($stamp.Backlot -ne $state.Head) -or (($stamp.Keys -join " ") -ne ($Keys -join " "))
        if ($restamped) {
            Write-StageStamp $stampFile $state.Head $Keys
            Write-Host "stamp: backlot $($state.Head), keys $($Keys -join ' ')"
        }
        # the other backend's pictures were taken at the old commit
        $metal = Get-ChildItem -ErrorAction SilentlyContinue (Join-Path $goldenRoot "StageEditor\*.metal.png")
        if ($stamp -and $stamp.Backlot -ne $state.Head -and $metal) {
            Write-Host "note: every StageEditor\*.metal.png is stale until it is recorded again on the Mac"
        }
    }

    $toleranceText = if ($Tolerance -ge 0) { "$Tolerance" } else { "the tool's default" }
    $recordedText = if ($Record) { ", $recorded recorded" } else { "" }
    Write-Host "stage_cuts: $captured captured, $compared compared, $same same, $different different, $missing missing$recordedText; $backend, tolerance $toleranceText"
    if ($skip) {
        Write-Host "SKIP: $skip; captured into $Out, compared nothing. Once the new pictures are right: Tools/stage_cuts.ps1 -Record"
        $exitCode = 77
    }
    elseif ($different -gt 0 -or $missing -gt 0) {
        if ($missing -gt 0) {
            Write-Host "to record the missing goldens: Tools/stage_cuts.ps1 -Record"
        }
        $exitCode = 1
    }
}
finally {
    Stop-StageEditor $process
}
Write-Host "cuts: $Out"
exit $exitCode
