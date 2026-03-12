# Check-Cpp.ps1
# Performs static analysis on C++ files

param(
    [Parameter(Mandatory=$true)]
    [string]$FilePath
)

if (-not (Test-Path $FilePath)) {
    Write-Error "File not found: $FilePath"
    exit 1
}

$issues = @()
$braceCount = 0
$parenCount = 0
$bracketCount = 0
$inBlockComment = $false

$lines = Get-Content $FilePath
for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    $lineNum = $i + 1
    
    # Handle block comments
    if ($line -match '/\*') {
        if (-not ($line -match '\*/')) {
            $inBlockComment = $true
        }
        continue
    }
    if ($line -match '\*/') {
        $inBlockComment = $false
        continue
    }
    if ($inBlockComment) { continue }
    
    # Skip pure comment lines
    if ($line.Trim().StartsWith('//')) { continue }
    
    # Count brackets
    for ($j = 0; $j -lt $line.Length; $j++) {
        $char = $line[$j]
        if ($char -eq '"' -or $char -eq "'") {
            $j++
            while ($j -lt $line.Length -and $line[$j] -ne $char) {
                if ($line[$j] -eq '\') { $j++ }
                $j++
            }
            continue
        }
        
        switch ($char) {
            '{' { $braceCount++ }
            '}' { $braceCount-- }
            '(' { $parenCount++ }
            ')' { $parenCount-- }
            '[' { $bracketCount++ }
            ']' { $bracketCount-- }
        }
    }
    
    # Check for TODO/FIXME
    if ($line -match 'TODO|FIXME') {
        $issues += @{ Line = $lineNum; Type = "note"; Message = "TODO/FIXME found"; Content = $line.Trim().Substring(0, [Math]::Min(80, $line.Trim().Length)) }
    }
    
    # Check for hardcoded Windows paths
    if ($line -match '[A-Z]:\\|\\\\' -and -not $line.Contains('#include')) {
        $issues += @{ Line = $lineNum; Type = "warning"; Message = "Possible hardcoded Windows path"; Content = $line.Trim().Substring(0, [Math]::Min(80, $line.Trim().Length)) }
    }
    
    # Check for using namespace in header
    if (($FilePath.EndsWith('.h') -or $FilePath.EndsWith('.hpp')) -and $line -match 'using\s+namespace\s+') {
        $issues += @{ Line = $lineNum; Type = "warning"; Message = "using namespace in header file"; Content = $line.Trim().Substring(0, [Math]::Min(80, $line.Trim().Length)) }
    }
}

# Check for unbalanced brackets
if ($braceCount -ne 0) {
    $issues += @{ Line = $lines.Count; Type = "error"; Message = "Unclosed braces: $braceCount" }
}
if ($parenCount -ne 0) {
    $issues += @{ Line = $lines.Count; Type = "error"; Message = "Unclosed parentheses: $parenCount" }
}
if ($bracketCount -ne 0) {
    $issues += @{ Line = $lines.Count; Type = "error"; Message = "Unclosed brackets: $bracketCount" }
}

$errors = ($issues | Where-Object { $_.Type -eq "error" }).Count
$warnings = ($issues | Where-Object { $_.Type -eq "warning" }).Count
$notes = ($issues | Where-Object { $_.Type -eq "note" }).Count

$result = @{
    FilePath = $FilePath
    TotalLines = $lines.Count
    IssuesFound = $issues.Count
    Errors = $errors
    Warnings = $warnings
    Notes = $notes
    Issues = $issues
    Status = if ($errors -gt 0) { "has_errors" } elseif ($warnings -gt 0) { "has_warnings" } else { "clean" }
    Summary = if ($errors -gt 0) { "Found $errors errors, $warnings warnings, $notes notes" } elseif ($issues.Count -gt 0) { "Found $warnings warnings and $notes notes - no errors" } else { "No issues detected" }
}

$result | ConvertTo-Json -Depth 3