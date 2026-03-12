# Search-Symbols.ps1
# Searches for C++ symbols (classes, functions, variables)

param(
    [Parameter(Mandatory=$true)]
    [string]$Symbol,
    
    [string]$Type = "all"
)

$results = @()

# Build search pattern based on type
$pattern = switch ($Type) {
    "class" { "\b(class|struct)\s+$Symbol\b" }
    "function" { "\b$Symbol\s*\(" }
    default { "\b$Symbol\b" }
}

$files = Get-ChildItem -Recurse -Include @("*.cpp", "*.h", "*.hpp", "*.c") -ErrorAction SilentlyContinue

foreach ($file in $files) {
    $content = Get-Content $file.FullName
    for ($i = 0; $i -lt $content.Count; $i++) {
        $line = $content[$i]
        if ($line -match $pattern) {
            $resultType = "usage"
            if ($line -match "\b(class|struct)\s+$Symbol\b") { $resultType = "class_definition" }
            elseif ($line -match "\b$Symbol\s*\([^)]*\)\s*(const)?\s*\{") { $resultType = "function_definition" }
            elseif ($line -match "\b$Symbol\s*\("") { $resultType = "function_call" }
            
            $results += @{
                File = $file.FullName.Replace((Get-Location).Path, "").TrimStart('\', '/')
                Line = $i + 1
                Type = $resultType
                Preview = $line.Trim().Substring(0, [Math]::Min(100, $line.Trim().Length))
            }
        }
    }
}

# Sort by priority: definitions first
$priority = @{ "class_definition" = 0; "function_definition" = 1; "declaration" = 2; "function_call" = 3; "usage" = 4 }
$sortedResults = $results | Sort-Object { $priority[$_.Type] }

$output = @{
    Symbol = $Symbol
    SearchType = $Type
    TotalFound = $results.Count
    Results = $sortedResults | Select-Object -First 20
    Summary = if ($results.Count -gt 0) { "Found $($results.Count) occurrences of '$Symbol'" } else { "No occurrences of '$Symbol' found" }
}

$output | ConvertTo-Json -Depth 3