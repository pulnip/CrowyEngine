# Fails when a C++ file under Engine/ differs from what clang-format makes
# of it.
#
# usage: Tools/format_check.ps1
#
# Same contract as Tools/format_check.sh: every .cpp, .hpp and .h below
# Engine/ is checked with clang-format --dry-run -Werror.
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$engine = Join-Path $repoRoot "Engine"

$files = @(
    Get-ChildItem -Path $engine -Recurse -File -Include *.cpp, *.hpp, *.h |
        ForEach-Object { $_.FullName }
)
if ($files.Count -eq 0) {
    Write-Host "FAIL: no C++ file under Engine"
    exit 1
}

# clang-format reports each drift on stderr, which must not stop this script;
# batches keep the command line under the Windows limit
$ErrorActionPreference = "Continue"
$failed = $false
$output = @()
for ($i = 0; $i -lt $files.Count; $i += 100) {
    $batch = $files[$i..([Math]::Min($i + 99, $files.Count - 1))]
    $output += & clang-format --dry-run -Werror $batch 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) {
        $failed = $true
    }
}

if ($failed) {
    # one line per file, not one per violation
    $drifted = @(
        $output |
            Where-Object { $_ -match "^(.+):\d+:\d+: error:" } |
            ForEach-Object { $Matches[1] } |
            Select-Object -Unique
    )
    if ($drifted.Count -eq 0) {
        $output | Select-Object -Last 20 | ForEach-Object { Write-Host $_ }
        Write-Host "FAIL: clang-format did not run cleanly"
        exit 1
    }
    foreach ($file in $drifted) {
        if ($file.StartsWith("$repoRoot\")) {
            $file = $file.Substring($repoRoot.Length + 1)
        }
        Write-Host $file
    }
    Write-Host "FAIL: $($drifted.Count) of $($files.Count) files differ; run clang-format -i on them"
    exit 1
}
Write-Host "format_check: $($files.Count) files in Engine match clang-format"
exit 0
