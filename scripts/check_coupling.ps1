$ErrorActionPreference = "Stop"

$budgetFile = "docs\coupling_budget.txt"
if (-not (Test-Path $budgetFile)) {
    Write-Error "Budget file not found: $budgetFile"
    exit 1
}

$budget = @{}
Get-Content $budgetFile | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        $budget[$matches[1]] = [int]$matches[2]
    }
}

$failed = $false

# 1. Singletons
$singletons = (Get-ChildItem -Path src -Recurse -Filter *.h | Select-String -Pattern "static\s+\w+\s*&?\s*Get\(\)").Count
if ($singletons -gt $budget["singletons"]) {
    Write-Host "FAIL: Singletons exceeded budget. Expected <= $($budget['singletons']), got $singletons" -ForegroundColor Red
    $failed = $true
} else {
    Write-Host "PASS: Singletons: $singletons (Budget: $($budget['singletons']))" -ForegroundColor Green
}

# 2. Enum Leaks
$enum_leaks = (Get-ChildItem -Path src -Recurse -File | Select-String -Pattern "GraphicsBackend::" | Select-Object -ExpandProperty Path -Unique | Where-Object { $_ -notmatch 'src[\\/]rendering' }).Count
if ($enum_leaks -gt $budget["backend_enum_leaks"]) {
    Write-Host "FAIL: Backend enum leaks exceeded budget. Expected <= $($budget['backend_enum_leaks']), got $enum_leaks" -ForegroundColor Red
    $failed = $true
} else {
    Write-Host "PASS: Backend enum leaks: $enum_leaks (Budget: $($budget['backend_enum_leaks']))" -ForegroundColor Green
}

# 3. Hardcoded Games
$games = (Get-ChildItem -Path src -Recurse -Filter *.cpp | Select-String -Pattern '"(hogwarts|cyberpunk|skyrim|elden|witcher|fallout)').Count
if ($games -gt $budget["hardcoded_game_names"]) {
    Write-Host "FAIL: Hardcoded game names exceeded budget. Expected <= $($budget['hardcoded_game_names']), got $games" -ForegroundColor Red
    $failed = $true
} else {
    Write-Host "PASS: Hardcoded game names: $games (Budget: $($budget['hardcoded_game_names']))" -ForegroundColor Green
}

# 4. Test Coverage
$test_sources = (Get-ChildItem -Path tests -Recurse -Filter *.cpp).Count
if ($test_sources -lt $budget["test_sources"]) {
    Write-Host "FAIL: Test sources dropped below budget. Expected >= $($budget['test_sources']), got $test_sources" -ForegroundColor Red
    $failed = $true
} else {
    Write-Host "PASS: Test sources: $test_sources (Budget: $($budget['test_sources']))" -ForegroundColor Green
}

$ctest_out = (ctest --test-dir build -N 2>$null | Select-Object -Last 1)
if ($ctest_out -match 'Total Tests:\s*(\d+)') {
    $test_registered = [int]$matches[1]
    if ($test_registered -lt $budget["test_registered"]) {
        Write-Host "FAIL: Registered tests dropped below budget. Expected >= $($budget['test_registered']), got $test_registered" -ForegroundColor Red
        $failed = $true
    } else {
        Write-Host "PASS: Registered tests: $test_registered (Budget: $($budget['test_registered']))" -ForegroundColor Green
    }
} else {
    Write-Host "WARN: Could not parse ctest output." -ForegroundColor Yellow
}

# 5. FrameCoordinator Degree
$graphify_out = (graphify explain "FrameCoordinator" | Select-String "Degree")
if ($graphify_out -match 'Degree:\s*(\d+)') {
    $degree = [int]$matches[1]
    if ($degree -gt $budget["framecoordinator_degree"]) {
        Write-Host "FAIL: FrameCoordinator degree exceeded budget. Expected <= $($budget['framecoordinator_degree']), got $degree" -ForegroundColor Red
        $failed = $true
    } else {
        Write-Host "PASS: FrameCoordinator degree: $degree (Budget: $($budget['framecoordinator_degree']))" -ForegroundColor Green
    }
} else {
    Write-Host "WARN: Could not parse graphify output." -ForegroundColor Yellow
}

if ($failed) {
    Write-Error "One or more coupling ratchets failed."
    exit 1
}

Write-Host "All ratchets passed!" -ForegroundColor Green
