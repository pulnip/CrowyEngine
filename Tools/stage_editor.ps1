# StageEditor driven over its command port, shared by stage_cuts.ps1 and
# stage_task.ps1; dot-source it after port.ps1.

# a path against PowerShell's location, which .NET does not follow
function Resolve-LocalPath([string]$Path) {
    return $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
}

# what the editor must not inherit: a debug override or a frame dump would
# change the pictures a script compares
$StageEditorScrubbed = @("CROWY_DEBUG", "CROWY_DUMP_FRAME", "CROWY_DUMP_FRAME_AT")

# launches the editor held, on `Port` alone, and waits until it answers
# there; an editor that exits 77 (its content missing) ends the script with 77
function Start-StageEditor([string]$Exe, [string]$WorkingDirectory, [int]$Port) {
    if (Get-Process -Name StageEditor -ErrorAction SilentlyContinue) {
        throw "a StageEditor is already running; close it first"
    }
    # the child alone takes the port, and without retries
    $saved = @{}
    foreach ($name in @("CROWY_COMMAND_PORT") + $StageEditorScrubbed) {
        $saved[$name] = [Environment]::GetEnvironmentVariable($name)
        [Environment]::SetEnvironmentVariable($name, $null)
    }
    $env:CROWY_COMMAND_PORT = "$Port"
    try {
        $process = Start-Process -FilePath $Exe -ArgumentList "--hold" -PassThru -WorkingDirectory $WorkingDirectory
    }
    finally {
        foreach ($name in $saved.Keys) {
            [Environment]::SetEnvironmentVariable($name, $saved[$name])
        }
    }
    # cached now, or ExitCode reads empty once the process has gone
    $null = $process.Handle
    $deadline = (Get-Date).AddSeconds(120)
    try {
        while ($true) {
            if ($process.HasExited) {
                if ($process.ExitCode -eq 77) {
                    Write-Host "SKIP: StageEditor exited 77: the content it draws is missing"
                    exit 77
                }
                throw "StageEditor exited $($process.ExitCode) before its port answered"
            }
            try {
                $ping = Invoke-Port ping
                break
            }
            catch {
                if ((Get-Date) -gt $deadline) {
                    throw "port: no answer on $Port within 120 s"
                }
                Start-Sleep -Milliseconds 250
            }
        }
        if ($ping.app -ne "StageEditor") {
            throw "port $Port answers for '$($ping.app)', not StageEditor"
        }
    }
    catch {
        Stop-StageEditor $process
        throw
    }
    return $process
}

# quits the editor over the port when it is the one answering, else ends it
function Stop-StageEditor($Process) {
    try {
        if ((Invoke-Port ping).app -eq "StageEditor") {
            $null = Invoke-Port quit
        }
    }
    catch {
    }
    if (-not $Process.WaitForExit(20000)) {
        Stop-Process -Id $Process.Id -Force
    }
}

# the editor reverts a write it cannot apply; read it back
function Set-EditorField([string]$Field, [string]$Value) {
    Set-PortProperty editor $Field $Value
    $now = Get-PortProperty editor $Field
    if ($now -ne $Value) {
        throw "editor.$Field stayed '$now': $(Get-PortProperty editor status)"
    }
}

# captures to `Bmp`, converted to a PNG beside it when the tool is there;
# returns the file that stays
function Save-StageCapture([string]$Bmp, [string]$Tool, [int]$Frame = 0) {
    Save-PortFrame $Bmp $Frame
    if (-not (Test-Path $Bmp)) {
        throw "the capture $Bmp was not written"
    }
    if (-not (Test-Path $Tool)) {
        return $Bmp
    }
    $png = [IO.Path]::ChangeExtension($Bmp, ".png")
    & $Tool --convert $Bmp $png | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "ImageCompareCheck could not convert $Bmp"
    }
    Remove-Item $Bmp
    return $png
}

# runs ImageCompareCheck and returns its exit code; the tool reports errors
# on stderr, which must not stop a script that runs under Stop
function Invoke-ImageCompareCheck([string]$Tool, [string[]]$Arguments, [switch]$Quiet) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $lines = & $Tool $Arguments 2>&1
        if (-not $Quiet) {
            foreach ($line in $lines) {
                Write-Host $line
            }
        }
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

# the content root of the scene file the editor launched with: <root>/Data/scene.json
function Get-StageRoot([string]$SceneFile) {
    return Split-Path -Parent (Split-Path -Parent $SceneFile)
}

# Backlot's commit and its tracked changes under what the editor reads;
# Head is empty when git cannot read `Root`
function Get-BacklotState([string]$Root) {
    $state = [pscustomobject]@{ Head = ""; Changes = @() }
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        return $state
    }
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $head = & git -C $Root rev-parse HEAD 2>$null
        if ($LASTEXITCODE -ne 0 -or -not $head) {
            return $state
        }
        $state.Head = "$head".Trim()
        $state.Changes = @(& git -C $Root status --porcelain --untracked-files=no -- Data Unity/Assets/Art 2>$null | Where-Object { $_ })
        return $state
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

# the Backlot commit StageEditor's goldens were taken at and the keys they
# pin; $null without a stamp
function Read-StageStamp([string]$Path) {
    if (-not (Test-Path $Path)) {
        return $null
    }
    $stamp = [pscustomobject]@{ Backlot = ""; Keys = @() }
    foreach ($line in (Get-Content -Encoding UTF8 $Path)) {
        if ($line -match '^backlot (\S+)') {
            $stamp.Backlot = $Matches[1]
        }
        elseif ($line -match '^keys (.+)$') {
            $stamp.Keys = @($Matches[1] -split '\s+' | Where-Object { $_ })
        }
    }
    return $stamp
}

function Write-StageStamp([string]$Path, [string]$Backlot, [string[]]$Keys) {
    $text = @(
        "# StageEditor's goldens: the Backlot commit they were taken at and the",
        "# lighting keys pinned for every cut; the launch picture is StageEditor.<backend>.png",
        "backlot $Backlot",
        "keys $($Keys -join ' ')"
    ) -join "`n"
    [IO.File]::WriteAllText($Path, "$text`n")
}
