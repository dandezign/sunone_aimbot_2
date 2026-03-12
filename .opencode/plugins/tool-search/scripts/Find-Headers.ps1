# Find-Headers.ps1
# Extracts #include dependencies from C++ source files

param(
    [Parameter(Mandatory=$true)]
    [string]$FilePath
)

if (-not (Test-Path $FilePath)) {
    Write-Error "File not found: $FilePath"
    exit 1
}

$content = Get-Content $FilePath -Raw
$includes = [regex]::Matches($content, '#include\s+[<"]([^>"]+)[>"]') | ForEach-Object { $_.Groups[1].Value }

$headers = @{
    FilePath = $FilePath
    HeadersFound = $includes.Count
    Headers = @()
    SystemHeaders = @()
    LocalHeaders = @()
    StandardLibrary = @()
    ThirdParty = @()
}

$standardLibs = @('iostream', 'vector', 'string', 'map', 'set', 'unordered_map', 'unordered_set', 
                  'memory', 'algorithm', 'functional', 'chrono', 'thread', 'mutex', 'future', 
                  'filesystem', 'optional', 'variant', 'any', 'array', 'list', 'deque', 'queue', 
                  'stack', 'utility', 'tuple', 'cstddef', 'cstdint', 'cmath', 'cstdlib')

foreach ($header in $includes) {
    $headerInfo = @{
        Name = $header
        IsLocal = $header.Contains('/') -or $header.Contains('\')
    }
    
    $headers.Headers += $headerInfo
    
    if ($headerInfo.IsLocal) {
        $headers.LocalHeaders += $header
    } else {
        $headers.SystemHeaders += $header
        if ($standardLibs -contains $header) {
            $headers.StandardLibrary += $header
        } else {
            $headers.ThirdParty += $header
        }
    }
}

$headers | ConvertTo-Json -Depth 3