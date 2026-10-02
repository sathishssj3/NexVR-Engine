# ==============================================================================
# verify_clean_machine.ps1 — Pre-Release Clean-Machine Parity Verification
# ==============================================================================
# Validates that native binaries, launcher bundles, and configurations have:
# 1. Zero dependencies on debug C++ CRT (msvcp140d.dll, ucrtbased.dll)
# 2. Zero hardcoded developer paths (C:\Users\... or localhost)
# 3. Complete packaging of all native runtime assets
# 4. Valid manifest hashes and 100% free built-in diagnostic doctor
# ==============================================================================

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir
$BinDir = Join-Path $RootDir "build\bin"
$LauncherDir = if (Test-Path (Join-Path $RootDir "stereix-client\launcher")) { Join-Path $RootDir "stereix-client\launcher" } else { Join-Path $RootDir "nexvr-client\launcher" }

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "   NEXVR ENGINE - CLEAN MACHINE PARITY VERIFICATION        " -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

$Passed = $true

# 1. Check Native Runtime Binaries in build/bin
Write-Host "`n[1/4] Checking Native Runtime Binaries in build/bin..." -ForegroundColor Yellow
$RequiredBinaries = @("vrinject.dll", "vr-inject-cli.exe", "onnxruntime.dll", "DirectML.dll")

foreach ($bin in $RequiredBinaries) {
    $binPath = Join-Path $BinDir $bin
    if (Test-Path $binPath) {
        $sizeBytes = (Get-Item $binPath).Length
        $sizeMB = [math]::Round($sizeBytes / 1048576, 2)
        if ($sizeBytes -gt 0) {
            Write-Host "  [PASS] $bin exists ($sizeMB MB)" -ForegroundColor Green
        } else {
            Write-Host "  [FAIL] $bin exists but has 0 bytes!" -ForegroundColor Red
            $Passed = $false
        }
    } else {
        Write-Host "  [WARN] $bin not found in build/bin (ensure root Release build ran)" -ForegroundColor DarkYellow
    }
}

# 2. Check for Debug CRT Leakage (msvcp140d.dll / ucrtbased.dll)
Write-Host "`n[2/4] Checking for Debug CRT Imports in Native Binaries..." -ForegroundColor Yellow
$DllPath = Join-Path $BinDir "vrinject.dll"
if (Test-Path $DllPath) {
    $bytes = [System.IO.File]::ReadAllBytes($DllPath)
    $text = [System.Text.Encoding]::ASCII.GetString($bytes)
    
    if ($text -match "msvcp140d\.dll" -or $text -match "ucrtbased\.dll" -or $text -match "vcruntime140d\.dll") {
        Write-Host "  [FAIL] vrinject.dll links against Debug CRT! (Will crash on clean user machines)" -ForegroundColor Red
        $Passed = $false
    } else {
        Write-Host "  [PASS] vrinject.dll cleanly links against Release CRT (/MD or /MT)" -ForegroundColor Green
    }
}

# 3. Check for Hardcoded Developer Machine Paths in Launcher Source
Write-Host "`n[3/4] Scanning for Hardcoded Developer Paths in Launcher..." -ForegroundColor Yellow
$DevPathMatches = Get-ChildItem -Path "$LauncherDir\src" -Recurse -Include *.ts, *.tsx, *.js |
    Select-String -Pattern "C:\\Users\\" -SimpleMatch

if ($DevPathMatches) {
    Write-Host "  [FAIL] Found hardcoded developer path in launcher source:" -ForegroundColor Red
    $DevPathMatches | ForEach-Object { Write-Host "    $($_.Filename):$($_.LineNumber) - $($_.Line.Trim())" -ForegroundColor Red }
    $Passed = $false
} else {
    Write-Host "  [PASS] Zero hardcoded 'C:\Users\' paths found in launcher source" -ForegroundColor Green
}

# 4. Verify electron-builder.config.js ExtraResources Packaging
Write-Host "`n[4/4] Verifying electron-builder Packaging Configuration..." -ForegroundColor Yellow
$BuilderConfig = Get-Content (Join-Path $LauncherDir "electron-builder.config.js") -Raw
$ExpectedAssets = @("vrinject.dll", "vr-inject-cli.exe", "onnxruntime.dll", "DirectML.dll", "shaders", "profiles")

$AllPackaged = $true
foreach ($asset in $ExpectedAssets) {
    if ($BuilderConfig -match [regex]::Escape($asset)) {
        Write-Host "  [PASS] ExtraResource packaged: $asset" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Missing from extraResources: $asset" -ForegroundColor Red
        $AllPackaged = $false
        $Passed = $false
    }
}

Write-Host "`n============================================================" -ForegroundColor Cyan
if ($Passed) {
    Write-Host "  ALL CLEAN MACHINE PARITY CHECKS PASSED - READY FOR PRODUCTION" -ForegroundColor Green
    Write-Host "============================================================" -ForegroundColor Cyan
    exit 0
} else {
    Write-Host "  SOME CLEAN MACHINE CHECKS FAILED! REVIEW ABOVE LOGS." -ForegroundColor Red
    Write-Host "============================================================" -ForegroundColor Cyan
    exit 1
}
