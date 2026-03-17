# Verify Superpowers OpenCode Setup (PowerShell)

Write-Host "=== Verifying Superpowers Setup ===" -ForegroundColor Cyan
Write-Host ""

$errors = 0

# Check plugin exists
$pluginPath = ".opencode\plugins\superpowers.js"
if (Test-Path $pluginPath) {
    Write-Host "✅ Plugin found: $pluginPath" -ForegroundColor Green
} else {
    Write-Host "❌ Plugin missing: $pluginPath" -ForegroundColor Red
    $errors++
}

# Check using-superpowers skill exists
$bootstrapSkill = ".opencode\skills\superpowers\using-superpowers\SKILL.md"
if (Test-Path $bootstrapSkill) {
    Write-Host "✅ Bootstrap skill found" -ForegroundColor Green
} else {
    Write-Host "❌ Bootstrap skill missing" -ForegroundColor Red
    $errors++
}

# Count skills
$skills = Get-ChildItem -Path ".opencode\skills\superpowers" -Directory -ErrorAction SilentlyContinue
$skillCount = 0
$skillList = @()

foreach ($skill in $skills) {
    $skillFile = Join-Path $skill.FullName "SKILL.md"
    if (Test-Path $skillFile) {
        $skillCount++
        $skillList += $skill.Name
    }
}

Write-Host "✅ Found $skillCount skills" -ForegroundColor Green

# Check agent exists
$agentPath = ".opencode\agents\code-reviewer.md"
if (Test-Path $agentPath) {
    Write-Host "✅ Code reviewer agent found" -ForegroundColor Green
} else {
    Write-Host "❌ Code reviewer agent missing" -ForegroundColor Red
    $errors++
}

Write-Host ""
Write-Host "=== Skills Installed ===" -ForegroundColor Cyan
foreach ($name in ($skillList | Sort-Object)) {
    Write-Host "  • $name" -ForegroundColor Gray
}

Write-Host ""
if ($errors -eq 0) {
    Write-Host "=== Setup Complete ✅ ===" -ForegroundColor Green
    Write-Host "Restart OpenCode to load the plugin." -ForegroundColor Yellow
} else {
    Write-Host "=== Setup Incomplete ❌ ===" -ForegroundColor Red
    Write-Host "Found $errors error(s). Please fix above issues." -ForegroundColor Red
}
