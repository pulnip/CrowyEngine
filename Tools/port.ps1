# The command port from PowerShell 5.1: dot-source it, then call the
# functions. Debug builds only; the port listens on 127.0.0.1:27500
# (CROWY_COMMAND_PORT overrides, or Set-PortNumber after dot-sourcing).
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

function Set-PortNumber([int]$Number) {
    $script:PortNumber = $Number
    $script:PortUri = "http://127.0.0.1:$Number/rpc"
}

if ($env:CROWY_COMMAND_PORT) {
    Set-PortNumber ([int]$env:CROWY_COMMAND_PORT)
}
else {
    Set-PortNumber 27500
}

# one verb; returns its result, throws its error
function Invoke-Port([string]$Command, [hashtable]$Arguments = @{}) {
    $body = @{ cmd = $Command; args = $Arguments } | ConvertTo-Json -Depth 8 -Compress
    $answer = Invoke-RestMethod -Method Post -Uri $script:PortUri -Body $body -ContentType "application/json"
    if (-not $answer.ok) {
        throw "port: $Command failed: $($answer.error)"
    }
    return $answer.result
}

# polls ping until the port answers or the timeout runs out; returns the ping
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

# captures `Frame`, or the next frame the loop starts, to `Path` (absolute:
# the app writes it); runs a held loop up to it and returns once the file is
# written, or throws when the write failed
function Save-PortFrame([string]$Path, [int]$Frame = 0) {
    $status = Invoke-Port ping
    $failures = $status.captureFailures
    $arguments = @{ path = $Path }
    if ($Frame -gt 0) {
        $arguments.frame = $Frame
    }
    $Frame = @((Invoke-Port capture_frame $arguments).frames)[0]
    if ($status.held) {
        $null = Invoke-Port run @{ until = $Frame }
    }
    $null = Invoke-Port wait_frame @{ frame = $Frame }

    $deadline = (Get-Date).AddSeconds(30)
    while (($status = Invoke-Port ping).capturesPending -ne 0) {
        if ((Get-Date) -gt $deadline) {
            throw "port: the capture of frame $Frame is still pending after 30 s"
        }
        Start-Sleep -Milliseconds 50
    }
    if ($status.captureFailures -gt $failures) {
        throw "port: the capture of frame $Frame to $Path failed"
    }
}
