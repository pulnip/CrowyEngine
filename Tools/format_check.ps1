# Fails when a C++ file under a directory listed in Tools/formatted.txt
# differs from what clang-format makes of it.
#
# usage: Tools/format_check.ps1
#
# Same contract as Tools/format_check.sh: one directory per line, relative to
# the repository root; blank lines and lines starting with # are skipped.
# Every .cpp, .hpp and .h below a listed directory is checked with
# clang-format --dry-run -Werror. An empty list passes; a listed directory
# that does not exist fails.
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$listPath = Join-Path $PSScriptRoot "formatted.txt"

$directories = @(
    Get-Content $listPath |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith("#") }
)
if ($directories.Count -eq 0) {
    Write-Host "format_check: no directory listed in Tools/formatted.txt"
    exit 0
}

$files = @()
foreach ($directory in $directories) {
    $path = Join-Path $repoRoot $directory
    if (-not (Test-Path -PathType Container $path)) {
        Write-Host "FAIL: $directory is listed but is not a directory"
        exit 1
    }
    $files += Get-ChildItem -Path $path -Recurse -File -Include *.cpp, *.hpp, *.h |
        ForEach-Object { $_.FullName }
}
if ($files.Count -eq 0) {
    Write-Host "format_check: no C++ file under $($directories -join ', ')"
    exit 0
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
Write-Host "format_check: $($files.Count) files in $($directories -join ', ') match clang-format"
exit 0
