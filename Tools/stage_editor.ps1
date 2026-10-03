# StageEditor driven over its command port, shared by stage_cuts.ps1 and
# stage_task.ps1; dot-source it after port.ps1.

# a path against PowerShell's location, which .NET does not follow
function Resolve-LocalPath([string]$Path) {
    return $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
}

# launches the editor held, on `Port` alone, and waits until it answers there
function Start-StageEditor([string]$Exe, [string]$WorkingDirectory, [int]$Port) {
    if (Get-Process -Name StageEditor -ErrorAction SilentlyContinue) {
        throw "a StageEditor is already running; close it first"
    }
    # the child alone takes the port, and without retries
    $previous = $env:CROWY_COMMAND_PORT
    $env:CROWY_COMMAND_PORT = "$Port"
    try {
        $process = Start-Process -FilePath $Exe -ArgumentList "--hold" -PassThru -WorkingDirectory $WorkingDirectory
    }
    finally {
        $env:CROWY_COMMAND_PORT = $previous
    }
    try {
        $ping = Wait-Port 120
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
