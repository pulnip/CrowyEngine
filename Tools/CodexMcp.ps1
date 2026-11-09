$ErrorActionPreference = 'Stop'
[Console]::InputEncoding = New-Object System.Text.UTF8Encoding($false)
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)

$projectDir = Split-Path -Parent $PSScriptRoot
$codexHome = if ($env:CODEX_HOME) { $env:CODEX_HOME } else { Join-Path $env:USERPROFILE '.codex' }

function Send-Response($id, $result, $errorInfo = $null) {
    $response = @{ jsonrpc = '2.0'; id = $id }
    if ($null -ne $errorInfo) { $response.error = $errorInfo }
    else { $response.result = $result }
    [Console]::Out.WriteLine(($response | ConvertTo-Json -Depth 20 -Compress))
    [Console]::Out.Flush()
}

function Find-Codex {
    $command = Get-Command codex.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Source }
    $binDir = Join-Path $env:LOCALAPPDATA 'OpenAI\Codex\bin'
    $candidate = Get-ChildItem -Path $binDir -Filter codex.exe -Recurse -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($candidate) { return $candidate.FullName }
    throw 'Codex CLI was not found.'
}

function Invoke-Codex($prompt, $writable) {
    $codex = Find-Codex
    $mode = if ($writable) { 'workspace-write' } else { 'read-only' }
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $codex
    $start.Arguments = '-a never exec -C "' + $projectDir + '" -s ' + $mode + ' --skip-git-repo-check --ephemeral -'
    $start.WorkingDirectory = $projectDir
    $start.UseShellExecute = $false
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.CreateNoWindow = $true
    $start.EnvironmentVariables['CODEX_HOME'] = $codexHome
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $start
    if (-not $process.Start()) { throw 'Could not start Codex CLI.' }
    try {
        $utf8 = New-Object System.Text.UTF8Encoding($false)
        $outReader = [System.IO.StreamReader]::new($process.StandardOutput.BaseStream, $utf8)
        $errReader = [System.IO.StreamReader]::new($process.StandardError.BaseStream, $utf8)
        $stdout = $outReader.ReadToEndAsync()
        $stderr = $errReader.ReadToEndAsync()
        $inputBytes = $utf8.GetBytes($prompt)
        $process.StandardInput.BaseStream.Write($inputBytes, 0, $inputBytes.Length)
        $process.StandardInput.Close()
        if (-not $process.WaitForExit(600000)) {
            $process.Kill()
            throw 'Codex exceeded the 10-minute timeout.'
        }
        $output = $stdout.Result.Trim()
        if ($process.ExitCode -ne 0) {
            $details = $stderr.Result.Trim()
            throw "Codex exited with code $($process.ExitCode). $details"
        }
        if (-not $output) { $output = '(Codex returned no text.)' }
        if ($output.Length -gt 30000) { $output = $output.Substring(0, 30000) + "`n[Output truncated]" }
        return $output
    }
    finally { $process.Dispose() }
}

while ($null -ne ($line = [Console]::In.ReadLine())) {
    try {
        $request = $line | ConvertFrom-Json
        if (-not $request.PSObject.Properties['id']) { continue }
        switch ($request.method) {
            'initialize' {
                Send-Response $request.id @{
                    protocolVersion = $request.params.protocolVersion
                    capabilities = @{ tools = @{} }
                    serverInfo = @{ name = 'codex-cli-bridge'; version = '1.0.0' }
                }
            }
            'ping' { Send-Response $request.id @{} }
            'tools/list' {
                Send-Response $request.id @{ tools = @(
                    @{
                        name = 'ask_codex'
                        description = 'Ask Codex to inspect this project and answer a question without changing files.'
                        inputSchema = @{ type = 'object'; properties = @{ prompt = @{ type = 'string'; description = 'Question or task for Codex.' } }; required = @('prompt') }
                    },
                    @{
                        name = 'edit_with_codex'
                        description = 'Ask Codex to edit this project. Codex runs with workspace-write sandbox and no interactive approvals.'
                        inputSchema = @{ type = 'object'; properties = @{ prompt = @{ type = 'string'; description = 'Requested project change for Codex.' } }; required = @('prompt') }
                    }
                ) }
            }
            'tools/call' {
                $name = $request.params.name
                $prompt = $request.params.arguments.prompt
                if ($name -notin @('ask_codex', 'edit_with_codex') -or -not $prompt -or $prompt -isnot [string]) {
                    Send-Response $request.id $null @{ code = -32602; message = 'Invalid tool name or prompt.' }
                    continue
                }
                try {
                    $output = Invoke-Codex $prompt ($name -eq 'edit_with_codex')
                    Send-Response $request.id @{ content = @(@{ type = 'text'; text = $output }) }
                }
                catch {
                    Send-Response $request.id @{ content = @(@{ type = 'text'; text = $_.Exception.Message }); isError = $true }
                }
            }
            default { Send-Response $request.id $null @{ code = -32601; message = 'Method not found.' } }
        }
    }
    catch {
        [Console]::Error.WriteLine($_.Exception.Message)
    }
}
