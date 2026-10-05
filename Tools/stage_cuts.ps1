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
# -UnityRun <run> sets each picture against Unity's of the same name in
# <Backlot>/Captures/<run> (or a folder given as a path) by their edges,
# report only: <Out>/unity holds an overlay per picture, summary.tsv and
# report.html. Style differs by design; the rows point at placement and
# never change the exit status.
#
# usage: Tools/stage_cuts.ps1 [-Keys day,night] [-Cuts street,roof]
#        [-Out captures/stage/<stamp>] [-Exe build/bin/StageEditor.exe]
#        [-Port 27520] [-Tolerance N] [-Record] [-UnityRun <run>]
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
    [switch]$Record,
    [string]$UnityRun = ""
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

# one row per picture against Unity's of the same name: the fields of
# ImageCompareCheck --edges's row, or why there are none
function Measure-UnityRows($Captures, [string]$UnityDir, [string]$UnityOut) {
    $fields = @("edges_engine", "edges_unity", "engine_near_unity", "unity_near_engine", "worst_tile", "worst_tile_near", "worst_tile_edges", "shift", "shift_coincide", "zero_coincide")
    foreach ($capture in $Captures) {
        $row = [ordered]@{ name = $capture.Name; cut = $capture.Cut; key = $capture.Key; golden = $capture.Golden; unity = "missing" }
        foreach ($field in $fields) {
            $row[$field] = "-"
        }
        $reference = Join-Path $UnityDir "$($capture.Name).png"
        if (Test-Path $reference) {
            $overlay = Join-Path $UnityOut "$($capture.Name).edges.png"
            Remove-Item -Force -ErrorAction SilentlyContinue $overlay
            $previous = $ErrorActionPreference
            $ErrorActionPreference = "Continue"
            try {
                $lines = @(& $tool --edges $capture.Png $reference --overlay $overlay 2>&1 | ForEach-Object { "$_" })
                $code = $LASTEXITCODE
            }
            finally {
                $ErrorActionPreference = $previous
            }
            $values = $lines | Where-Object { $_ -like "row`t*" } | Select-Object -First 1
            if ($code -eq 0 -and $values) {
                $row.unity = "ok"
                $parts = $values -split "`t"
                for ($i = 0; $i -lt $fields.Count; ++$i) {
                    $row[$fields[$i]] = $parts[$i + 1]
                }
            }
            else {
                $message = $lines | Where-Object { $_ -like "error:*" } | Select-Object -First 1
                $row.unity = if ($message) { ($message -replace '^error:\s*', '' -replace '\s+', ' ') } else { "failed ($code)" }
            }
        }
        [pscustomobject]$row
    }
}

function ConvertTo-HtmlText([string]$Text) {
    return [System.Net.WebUtility]::HtmlEncode($Text)
}

# per cut in scene order and key in run order: the numbers, the engine's
# picture blinking against Unity's on hover, Unity's, and the overlay
function Write-UnityReport($Rows, [string]$Path, [string]$UnityDir, [string]$Header) {
    $html = New-Object System.Text.StringBuilder
    $null = $html.Append(@"
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>StageEditor against Unity</title>
<style>
body { font: 14px system-ui, sans-serif; margin: 16px; background: #111; color: #ddd; }
code { color: #fff; }
a { color: #8cf; }
table { border-collapse: collapse; margin: 8px 0 24px; }
th, td { padding: 2px 8px; border-bottom: 1px solid #333; text-align: right; white-space: nowrap; }
th:first-child, td:first-child { text-align: left; }
.pictures { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin: 4px 0 20px; }
.pictures img { width: 100%; display: block; }
.blink { position: relative; display: block; }
.blink img.unity { position: absolute; inset: 0; opacity: 0; }
.blink:hover img.unity { animation: blink 1s steps(1) infinite; }
@keyframes blink { 50% { opacity: 1; } }
.legend span { padding: 0 6px; }
</style>
</head>
<body>
<h1>StageEditor against Unity</h1>
$Header
<p>Report only: the style differs by design (sky gradient, bloom, tone mapping, SMAA), so these numbers point at placement and gate nothing. A Unity run must postdate the scene file.</p>
<p class="legend">Overlay: <span style="color:#a0a0a0">grey, edges both pictures have</span><span style="color:#f0f">magenta, the engine's alone</span><span style="color:#0f0">green, Unity's alone</span><span style="color:#ffd200">yellow, the worst tile</span>. Hover the engine's picture to blink it against Unity's.</p>
<table>
<tr><th>picture</th><th>golden</th><th>unity</th><th>engine near unity</th><th>unity near engine</th><th>worst tile</th><th>its share</th><th>shift</th><th>coincide</th><th>at (0,0)</th></tr>

"@)
    foreach ($row in $Rows) {
        $null = $html.Append("<tr><td><a href=`"#$(ConvertTo-HtmlText $row.name)`">$(ConvertTo-HtmlText $row.name)</a></td><td>$($row.golden)</td><td>$(ConvertTo-HtmlText $row.unity)</td><td>$($row.engine_near_unity)</td><td>$($row.unity_near_engine)</td><td>$($row.worst_tile)</td><td>$($row.worst_tile_near)</td><td>$($row.shift)</td><td>$($row.shift_coincide)</td><td>$($row.zero_coincide)</td></tr>`n")
    }
    $null = $html.Append("</table>`n")
    foreach ($cut in @($Rows | ForEach-Object { $_.cut } | Select-Object -Unique)) {
        $null = $html.Append("<h2>$(ConvertTo-HtmlText $cut)</h2>`n")
        foreach ($row in @($Rows | Where-Object { $_.cut -eq $cut })) {
            $escaped = [Uri]::EscapeDataString($row.name)
            $engine = "../$escaped.png"
            $unity = ([Uri](Join-Path $UnityDir "$($row.name).png")).AbsoluteUri
            $overlay = "$escaped.edges.png"
            $null = $html.Append("<h3 id=`"$(ConvertTo-HtmlText $row.name)`">$(ConvertTo-HtmlText $row.name): $(ConvertTo-HtmlText $row.unity), engine near unity $($row.engine_near_unity), unity near engine $($row.unity_near_engine), shift $($row.shift)</h3>`n")
            $null = $html.Append("<div class=`"pictures`">")
            $null = $html.Append("<a class=`"blink`" href=`"$engine`"><img src=`"$engine`" alt=`"engine`" loading=`"lazy`"><img class=`"unity`" src=`"$unity`" alt=`"`" loading=`"lazy`"></a>")
            $null = $html.Append("<a href=`"$unity`"><img src=`"$unity`" alt=`"Unity`" loading=`"lazy`"></a>")
            if ($row.unity -eq "ok") {
                $null = $html.Append("<a href=`"$overlay`"><img src=`"$overlay`" alt=`"overlay`" loading=`"lazy`"></a>")
            }
            $null = $html.Append("</div>`n")
        }
    }
    $null = $html.Append("</body>`n</html>`n")
    [IO.File]::WriteAllText($Path, $html.ToString())
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
    $unityDir = ""
    if ($UnityRun) {
        $unityDir = if (Test-Path -PathType Container $UnityRun) { (Resolve-Path $UnityRun).Path } else { Join-Path $root "Captures\$UnityRun" }
        if (-not (Test-Path -PathType Container $unityDir)) {
            Write-Host "unity: no run at $unityDir; the rows are skipped"
            $unityDir = ""
        }
    }
    $skip = ""
    $skipHint = "Once the new pictures are right: Tools/stage_cuts.ps1 -Record"
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
            $skipHint = "Goldens are stamped only from a git checkout of Backlot."
        }
        elseif ($state.Head -ne $stamp.Backlot) {
            $skip = "Backlot is at $head, the goldens were taken at $(Get-ShortHash $stamp.Backlot)"
        }
        elseif ($state.Changes.Count -gt 0) {
            $skip = "Backlot has uncommitted changes the editor reads ($($state.Changes -join '; '))"
            $skipHint = "Commit or revert them in Backlot first."
        }
    }

    $captures = New-Object System.Collections.Generic.List[object]
    $captured = 0
    $compared = 0
    $same = 0
    $different = 0
    $missing = 0
    $smokeMissing = 0
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
            $capture = [pscustomobject]@{ Name = $name; Cut = $cut; Key = $key; Png = $png; Golden = "skipped" }
            $captures.Add($capture)
            if ($skip) {
                Write-Host "captured  $name"
                continue
            }
            if ($pinned -notcontains $key) {
                $capture.Golden = "not pinned"
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
                    $capture.Golden = "recorded"
                    Write-Host "recorded  $name"
                    continue
                }
                ++$missing
                $capture.Golden = "missing"
                Write-Host "FAIL: no golden for $name on $backend ($golden)"
                if ($smoke) {
                    ++$smokeMissing
                    Write-Host "this is the smoke's golden, which -Record never writes: capture it with CROWY_SMOKE_CAPTURE_DIR set through Tools/smoke_run.ps1 build\bin\StageEditor.exe and copy the capture to $golden"
                }
                continue
            }

            ++$compared
            $result = Invoke-ImageCompareCheck $tool (@($png, $golden) + $compareArguments) -Quiet
            if ($result -eq 0) {
                ++$same
                $capture.Golden = "same"
                Write-Host "same      $name"
                continue
            }
            if ($result -eq 1 -and $Record -and -not $smoke) {
                Copy-Item -Force $png $golden
                ++$recorded
                $capture.Golden = "recorded"
                Write-Host "recorded  $name (it differed)"
                continue
            }
            ++$different
            $capture.Golden = "differs"
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
        # the stamp keeps every key it pinned; a run adds its own after them
        $stampKeys = if ($stamp) { @($stamp.Keys) } else { @() }
        $stampedKeys = @($stampKeys) + @($Keys | Where-Object { $stampKeys -notcontains $_ })
        $moved = $stamp -and $stamp.Backlot -ne $state.Head
        $restamped = (-not $stamp) -or $moved -or (($stampKeys -join " ") -ne ($stampedKeys -join " "))
        if ($restamped) {
            Write-StageStamp $stampFile $state.Head $stampedKeys
            Write-Host "stamp: backlot $($state.Head), keys $($stampedKeys -join ' ')"
        }
        $unrecorded = @($stampedKeys | Where-Object { $Keys -notcontains $_ })
        if ($moved -and ($unrecorded.Count -gt 0 -or $Cuts.Count -lt $sceneCuts.Count)) {
            Write-Host "note: the goldens this run did not record were taken at $(Get-ShortHash $stamp.Backlot); record them at this commit too"
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
        Write-Host "SKIP: $skip; captured into $Out, compared nothing. $skipHint"
        $exitCode = 77
    }
    elseif ($different -gt 0 -or $missing -gt 0) {
        if ($missing -gt $smokeMissing) {
            Write-Host "to record the missing goldens: Tools/stage_cuts.ps1 -Record"
        }
        $exitCode = 1
    }
    elseif (-not $Record -and $compared -eq 0) {
        Write-Host "SKIP: no key of this run is pinned (pinned: $($pinned -join ' ')); captured into $Out, compared nothing"
        $exitCode = 77
    }
}
finally {
    Stop-StageEditor $process
}
Write-Host "cuts: $Out"

if ($unityDir) {
    $unityOut = Join-Path $Out "unity"
    New-Item -ItemType Directory -Force -Path $unityOut | Out-Null
    $rows = @(Measure-UnityRows $captures $unityDir $unityOut)
    $columns = @("name", "cut", "key", "golden", "unity", "edges_engine", "edges_unity", "engine_near_unity", "unity_near_engine", "worst_tile", "worst_tile_near", "worst_tile_edges", "shift", "shift_coincide", "zero_coincide")
    $tsv = @($columns -join "`t") + @($rows | ForEach-Object { $row = $_; ($columns | ForEach-Object { $row.$_ }) -join "`t" })
    [IO.File]::WriteAllText((Join-Path $unityOut "summary.tsv"), ($tsv -join "`n") + "`n")

    $engineHead = (& git -C $repoRoot rev-parse --short HEAD 2>$null)
    $header = "<p>Engine <code>$engineHead</code>; Backlot <code>$(ConvertTo-HtmlText $root)</code> at <code>$head</code>, $clean; Unity run <code>$(ConvertTo-HtmlText $unityDir)</code>; the tool's edge defaults (radius 2, density 5%, lenience 2).</p>"
    Write-UnityReport $rows (Join-Path $unityOut "report.html") $unityDir $header

    Write-Host ""
    Write-Host "unity: $unityDir (report only)"
    Write-Host ("{0,-22} {1,-10} {2,-8} {3,7} {4,7}  {5,-6} {6,7}  {7,-6} {8,7} {9,7}" -f "picture", "golden", "unity", "e->u", "u->e", "tile", "share", "shift", "coin", "at 0")
    foreach ($row in $rows) {
        Write-Host ("{0,-22} {1,-10} {2,-8} {3,7} {4,7}  {5,-6} {6,7}  {7,-6} {8,7} {9,7}" -f $row.name, $row.golden, $row.unity, $row.engine_near_unity, $row.unity_near_engine, $row.worst_tile, $row.worst_tile_near, $row.shift, $row.shift_coincide, $row.zero_coincide)
    }
    Write-Host "unity report: $(Join-Path $unityOut 'report.html')"
}
exit $exitCode
