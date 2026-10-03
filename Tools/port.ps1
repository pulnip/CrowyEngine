# The command port from PowerShell 5.1: dot-source it, then call the
# functions. Debug builds only; the port listens on 127.0.0.1:27500
# (CROWY_COMMAND_PORT overrides).
#
#   . Tools/port.ps1
#   Wait-Port
#   Set-PortProperty editor cut street
#   Get-PortProperty camera position
#   Save-PortFrame captures/street.bmp
#   Invoke-Port quit
#
# Every function throws when the port answers with an error.

# the port does not answer .NET's Expect: 100-continue
[System.Net.ServicePointManager]::Expect100Continue = $false

$script:PortNumber = 27500
if ($env:CROWY_COMMAND_PORT) {
    $script:PortNumber = [int]$env:CROWY_COMMAND_PORT
}
$script:PortUri = "http://127.0.0.1:$($script:PortNumber)/rpc"

# one verb; returns its result, throws its error
function Invoke-Port([string]$Command, [hashtable]$Arguments = @{}) {
    $body = @{ cmd = $Command; args = $Arguments } | ConvertTo-Json -Depth 8 -Compress
    $answer = Invoke-RestMethod -Method Post -Uri $script:PortUri -Body $body -ContentType "application/json"
    if (-not $answer.ok) {
        throw "port: $Command failed: $($answer.error)"
    }
    return $answer.result
}

# polls ping until the port answers or the timeout runs out
function Wait-Port([int]$TimeoutSeconds = 60) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            return Invoke-Port ping
        }
        catch {
            Start-Sleep -Milliseconds 250
        }
    }
    throw "port: no answer on $($script:PortUri) within $TimeoutSeconds s"
}

function Set-PortProperty([string]$Target, [string]$Path, $Value) {
    $null = Invoke-Port set_property @{ target = $Target; path = $Path; value = $Value }
}

# the value itself, a leaf or a whole struct
function Get-PortProperty([string]$Target, [string]$Path = "") {
    $arguments = @{ target = $Target }
    if ($Path) {
        $arguments.path = $Path
    }
    return (Invoke-Port get_property $arguments).value
}

# captures the next frame (or `Frame`) to `Path`, runs the loop up to it,
# and returns once the file is written; works on a held loop
function Save-PortFrame([string]$Path, [int]$Frame = 0) {
    if ($Frame -le 0) {
        $Frame = (Invoke-Port ping).frame + 1
    }
    $null = Invoke-Port capture_frame @{ path = $Path; frame = $Frame }
    $null = Invoke-Port run @{ until = $Frame }
    $null = Invoke-Port wait_frame @{ frame = $Frame }
    while ((Invoke-Port ping).capturesPending -ne 0) {
        Start-Sleep -Milliseconds 50
    }
}
