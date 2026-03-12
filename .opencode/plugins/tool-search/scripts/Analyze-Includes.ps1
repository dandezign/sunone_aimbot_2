# Analyze-Includes.ps1
# Analyzes include structure for circular dependencies

param(
    [string]$EntryPoint = "."
)

$includeGraph = @{}
$circularDeps = @()

# Find all C++ files
$files = Get-ChildItem -Path $EntryPoint -Recurse -Include @("*.cpp", "*.h", "*.hpp") -ErrorAction SilentlyContinue | Select-Object -First 100

# Build include graph
foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if ($content) {
        $includes = [regex]::Matches($content, '#include\s+[<"]([^>"]+)[>"]') | ForEach-Object { $_.Groups[1].Value }
        $includeGraph[$file.FullName] = $includes
    }
}

# Detect cycles (simplified)
function Find-Cycles($node, $visited, $recStack, $path) {
    if ($recStack.Contains($node)) {
        $cycleStart = $path.IndexOf($node)
        return @($path[$cycleStart..($path.Count - 1)] + $node)
    }
    
    if ($visited.Contains($node)) { return @() }
    
    $visited.Add($node) | Out-Null
    $recStack.Add($node) | Out-Null
    $path.Add($node) | Out-Null
    
    $deps = $includeGraph[$node]
    if ($deps) {
        foreach ($dep in $deps) {
            $depFile = $files | Where-Object { $_.FullName -like "*$dep" -or $_.FullName -like "*/$dep" } | Select-Object -First 1
            if ($depFile) {
                $cycle = Find-Cycles $depFile.FullName $visited $recStack $path
                if ($cycle) { return $cycle }
            }
        }
    }
    
    $path.RemoveAt($path.Count - 1) | Out-Null
    $recStack.Remove($node) | Out-Null
    return @()
}

$visited = New-Object System.Collections.Generic.HashSet[string]
foreach ($file in $includeGraph.Keys) {
    $cycle = Find-Cycles $file $visited (New-Object System.Collections.Generic.HashSet[string]) (New-Object System.Collections.Generic.List[string])
    if ($cycle -and $circularDeps.Count -lt 5) {
        $circularDeps += @($cycle | ForEach-Object { $_.Replace((Get-Location).Path, "").TrimStart('\', '/').Split('\')[-1].Split('/')[-1] })
    }
}

# Calculate stats
$totalIncludes = 0
$systemHeaders = @()
$localHeaders = @()

foreach ($includes in $includeGraph.Values) {
    $totalIncludes += $includes.Count
    foreach ($inc in $includes) {
        if ($inc -match '[/\\]') { $localHeaders += $inc } else { $systemHeaders += $inc }
    }
}

$output = @{
    TotalFiles = $includeGraph.Count
    TotalIncludes = $totalIncludes
    UniqueSystemHeaders = ($systemHeaders | Select-Object -Unique).Count
    UniqueLocalHeaders = ($localHeaders | Select-Object -Unique).Count
    CircularDependencies = $circularDeps
    HasCircularDeps = $circularDeps.Count -gt 0
    Summary = if ($circularDeps.Count -gt 0) { "Found $($circularDeps.Count) potential circular dependencies in $($includeGraph.Count) files" } else { "No circular dependencies detected in $($includeGraph.Count) files" }
}

$output | ConvertTo-Json -Depth 3