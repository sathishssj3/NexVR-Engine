# scripts/package_unreal_plugin.ps1 — Packages the Unreal Engine Plugin for Marketplace / Partner distribution
[CmdletBinding()]
param(
    [string]$OutputDir = 'dist',
    [string]$Version = 'v0.3.0'
)

$ErrorActionPreference = 'Stop'

$RepoRoot = (Get-Item $PSScriptRoot).Parent.FullName
Set-Location $RepoRoot

$BuildBinDir = Join-Path $RepoRoot 'build\bin'
$AdapterSrcDir = if (Test-Path (Join-Path $RepoRoot 'stereix-client\src\adapters\unreal')) {
    Join-Path $RepoRoot 'stereix-client\src\adapters\unreal'
} elseif (Test-Path (Join-Path $RepoRoot 'src\adapters\unreal')) {
    Join-Path $RepoRoot 'src\adapters\unreal'
} else {
    Join-Path $RepoRoot 'nexvr-client\src\adapters\unreal'
}
$IncludeDir = Join-Path $RepoRoot 'include'
$DistDir = Join-Path $RepoRoot $OutputDir
$StagingDir = Join-Path $DistDir 'staging\Stereix'
$ZipName = 'Stereix_Unreal_Marketplace_Plugin_' + $Version + '.zip'
$ZipPath = Join-Path $DistDir $ZipName

Write-Host '==========================================================' -ForegroundColor Cyan
Write-Host (' NexVR Engine — Unreal Marketplace Plugin Packager (' + $Version + ')') -ForegroundColor Cyan
Write-Host '==========================================================' -ForegroundColor Cyan

# 1. Verify build artifacts
$RequiredBinaries = @(
    'nexvr_sdk.dll',
    'nexvr_sdk.lib',
    'nexvr_unreal.dll',
    'nexvr_unreal.lib'
)

Write-Host ('[1/5] Verifying native engine binaries in ' + $BuildBinDir + '...') -ForegroundColor Yellow
foreach ($bin in $RequiredBinaries) {
    $fullPath = Join-Path $BuildBinDir $bin
    if (-not (Test-Path $fullPath)) {
        throw ('Missing required binary: ' + $fullPath + '. Please build nexvr_sdk and nexvr_unreal first.')
    }
    Write-Host ('  [OK] ' + $bin) -ForegroundColor Green
}

# 2. Prepare staging directory
Write-Host '[2/5] Setting up staging directory...' -ForegroundColor Yellow
if (Test-Path $StagingDir) {
    Remove-Item -Recurse -Force $StagingDir
}
New-Item -ItemType Directory -Force -Path $StagingDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $StagingDir 'Config') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $StagingDir 'Resources') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $StagingDir 'Binaries\Win64') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $StagingDir 'Source\Stereix') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $StagingDir 'Source\ThirdParty\Stereix\include') | Out-Null

# 3. Copy files to staging
Write-Host '[3/5] Copying plugin sources, metadata, and binaries...' -ForegroundColor Yellow

# Plugin descriptors and docs
$pluginDesc = if (Test-Path (Join-Path $AdapterSrcDir 'Stereix.uplugin')) { 'Stereix.uplugin' } else { 'NexVR.uplugin' }
Copy-Item (Join-Path $AdapterSrcDir $pluginDesc) $StagingDir
Copy-Item (Join-Path $AdapterSrcDir 'README.md') $StagingDir
Copy-Item (Join-Path $AdapterSrcDir 'Config\FilterPlugin.ini') (Join-Path $StagingDir 'Config')
Copy-Item (Join-Path $AdapterSrcDir 'Resources\Icon128.png') (Join-Path $StagingDir 'Resources')

# Module source
$srcModuleDir = if (Test-Path (Join-Path $AdapterSrcDir 'Source\Stereix')) { 'Source\Stereix' } else { 'Source\NexVR' }
Copy-Item -Recurse -Force (Join-Path $AdapterSrcDir "$srcModuleDir\*") (Join-Path $StagingDir 'Source\Stereix')

# Headers
if (Test-Path (Join-Path $IncludeDir 'stereix_sdk.h')) {
    Copy-Item (Join-Path $IncludeDir 'stereix_sdk.h') (Join-Path $StagingDir 'Source\ThirdParty\Stereix\include')
}
if (Test-Path (Join-Path $IncludeDir 'nexvr_sdk.h')) {
    Copy-Item (Join-Path $IncludeDir 'nexvr_sdk.h') (Join-Path $StagingDir 'Source\ThirdParty\Stereix\include')
}
Copy-Item (Join-Path $AdapterSrcDir 'nexvr_unreal_bridge.h') (Join-Path $StagingDir 'Source\ThirdParty\Stereix\include')

# Binaries
foreach ($bin in $RequiredBinaries) {
    Copy-Item (Join-Path $BuildBinDir $bin) (Join-Path $StagingDir 'Binaries\Win64')
}

Write-Host '  Staged successfully.' -ForegroundColor Green

# 4. Create ZIP distribution
Write-Host '[4/5] Compressing standalone distribution archive...' -ForegroundColor Yellow
if (Test-Path $ZipPath) {
    Remove-Item -Force $ZipPath
}

Compress-Archive -Path $StagingDir -DestinationPath $ZipPath -CompressionLevel Optimal
Write-Host ('  Created: ' + $ZipPath) -ForegroundColor Green

# 5. Checksum and verification
Write-Host '[5/5] Computing SHA-256 integrity checksum...' -ForegroundColor Yellow
$hash = (Get-FileHash -Algorithm SHA256 $ZipPath).Hash
$fileItem = Get-Item $ZipPath
$sizeMb = [math]::Round($fileItem.Length / 1MB, 2)
Write-Host '==========================================================' -ForegroundColor Cyan
Write-Host (' Package Built: ' + $ZipName) -ForegroundColor Green
Write-Host (' Size:          ' + $sizeMb + ' MB') -ForegroundColor White
Write-Host (' SHA-256:       ' + $hash) -ForegroundColor White
Write-Host (' Destination:   ' + $ZipPath) -ForegroundColor White
Write-Host '==========================================================' -ForegroundColor Cyan

# Clean up staging
Remove-Item -Recurse -Force (Join-Path $DistDir 'staging')

Write-Host 'Marketplace package packaging completed successfully.' -ForegroundColor Green
