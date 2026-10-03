param (
    [string]$Version = "0.1.99",
    [string]$OutputDir = "C:\Users\sathi\.gemini\antigravity\scratch\Stereix-Engine\dist-release"
)

$ErrorActionPreference = 'Stop'

$RepoRoot = "C:\Users\sathi\.gemini\antigravity\scratch\Stereix-Engine"
$BinDir = "$RepoRoot\stereix-client\build\bin"
$LauncherDist = "$RepoRoot\stereix-client\launcher\dist-electron"

Write-Host "=========================================================="
Write-Host " Stereix Engine -- Production Packaging Pipeline ($Version)"
Write-Host "=========================================================="

if (Test-Path $OutputDir) {
    Remove-Item -Recurse -Force $OutputDir
}
New-Item -ItemType Directory -Path $OutputDir | Out-Null

# -------------------------------------------------------------------------
# 1. Package Standalone Runtime ZIP
# -------------------------------------------------------------------------
Write-Host ""
Write-Host ">>> [1/4] Building Standalone Engine Runtime ZIP..."
$StandaloneTemp = Join-Path $OutputDir "temp_standalone"
New-Item -ItemType Directory -Path $StandaloneTemp | Out-Null

$RuntimeFiles = @(
    @{ Src = "$BinDir\vrinject.dll"; Dst = "vrinject.dll" },
    @{ Src = "$BinDir\vr-inject-cli.exe"; Dst = "vr-inject-cli.exe" },
    @{ Src = "$BinDir\stereix_sdk.dll"; Dst = "stereix_sdk.dll" },
    @{ Src = "$BinDir\proxy\d3d11.dll"; Dst = "proxy\d3d11.dll" },
    @{ Src = "$BinDir\proxy\dxgi.dll"; Dst = "proxy\dxgi.dll" },
    @{ Src = "$BinDir\onnxruntime.dll"; Dst = "onnxruntime.dll" },
    @{ Src = "$BinDir\DirectML.dll"; Dst = "DirectML.dll" },
    @{ Src = "$BinDir\vrinject.json"; Dst = "vrinject.json" },
    @{ Src = "$RepoRoot\stereix-client\vr_inject_layer.json"; Dst = "vr_inject_layer.json" }
)

foreach ($item in $RuntimeFiles) {
    if (Test-Path $item.Src) {
        $destPath = Join-Path $StandaloneTemp $item.Dst
        $parent = Split-Path -Parent $destPath
        if (-not (Test-Path $parent)) { New-Item -ItemType Directory -Path $parent | Out-Null }
        Copy-Item -Path $item.Src -Destination $destPath -Force
        Write-Host "  Added: $($item.Dst)"
    } else {
        Write-Warning "  Missing runtime file: $($item.Src)"
    }
}

# Directories (shaders, models, profiles)
$RuntimeDirs = @(
    @{ Src = "$BinDir\shaders"; Dst = "shaders" },
    @{ Src = "$RepoRoot\stereix-client\models"; Dst = "models" },
    @{ Src = "$RepoRoot\stereix-client\profiles"; Dst = "profiles" }
)

foreach ($dir in $RuntimeDirs) {
    if (Test-Path $dir.Src) {
        $destPath = Join-Path $StandaloneTemp $dir.Dst
        Copy-Item -Path $dir.Src -Destination $destPath -Recurse -Force
        Write-Host "  Added directory: $($dir.Dst)"
    }
}

# Add documentation / license
Copy-Item "$RepoRoot\README.md" "$StandaloneTemp\README.md" -Force
if (Test-Path "$RepoRoot\LICENSE") { Copy-Item "$RepoRoot\LICENSE" "$StandaloneTemp\LICENSE" -Force }

$StandaloneZip = Join-Path $OutputDir "Stereix-Engine-v$Version-Standalone-Win64.zip"
Compress-Archive -Path "$StandaloneTemp\*" -DestinationPath $StandaloneZip -Force
Remove-Item -Recurse -Force $StandaloneTemp
Write-Host "  Package created: $StandaloneZip"

# -------------------------------------------------------------------------
# 2. Package Enterprise Developer SDK ZIP
# -------------------------------------------------------------------------
Write-Host ""
Write-Host ">>> [2/4] Building Enterprise Developer SDK ZIP..."
$SdkTemp = Join-Path $OutputDir "temp_sdk"
New-Item -ItemType Directory -Path $SdkTemp | Out-Null

# Headers
New-Item -ItemType Directory -Path "$SdkTemp\include" | Out-Null
Copy-Item "$RepoRoot\include\*" "$SdkTemp\include\" -Recurse -Force

# Libs
New-Item -ItemType Directory -Path "$SdkTemp\lib\x64" | Out-Null
$LibFiles = @("stereix_sdk.lib", "stereix_unity.lib", "stereix_unreal.lib", "openxr_loader.lib", "vrinject.lib", "d3d11.lib", "dxgi.lib")
foreach ($lib in $LibFiles) {
    $src = Join-Path $BinDir $lib
    if (Test-Path $src) {
        Copy-Item $src "$SdkTemp\lib\x64\" -Force
        Write-Host "  Added SDK Lib: $lib"
    }
}

# Binaries
New-Item -ItemType Directory -Path "$SdkTemp\bin\x64" | Out-Null
$BinFiles = @("stereix_sdk.dll", "stereix_unity.dll", "stereix_unreal.dll")
foreach ($bin in $BinFiles) {
    $src = Join-Path $BinDir $bin
    if (Test-Path $src) {
        Copy-Item $src "$SdkTemp\bin\x64\" -Force
        Write-Host "  Added SDK Binary: $bin"
    }
}

# Adapters
New-Item -ItemType Directory -Path "$SdkTemp\adapters" | Out-Null
Copy-Item "$RepoRoot\stereix-client\src\adapters\unity" "$SdkTemp\adapters\unity" -Recurse -Force
Copy-Item "$RepoRoot\stereix-client\src\adapters\unreal" "$SdkTemp\adapters\unreal" -Recurse -Force

# Samples
New-Item -ItemType Directory -Path "$SdkTemp\samples" | Out-Null
Copy-Item "$RepoRoot\samples\*" "$SdkTemp\samples\" -Recurse -Force

$SdkZip = Join-Path $OutputDir "Stereix-Engine-SDK-v$Version-Win64.zip"
Compress-Archive -Path "$SdkTemp\*" -DestinationPath $SdkZip -Force
Remove-Item -Recurse -Force $SdkTemp
Write-Host "  Package created: $SdkZip"

# -------------------------------------------------------------------------
# 3. Copy Signed Launcher Installers
# -------------------------------------------------------------------------
Write-Host ""
Write-Host ">>> [3/4] Collecting Signed Launcher Binaries..."
$LauncherInstallers = Get-ChildItem -Path "$LauncherDist\*" -Include "Stereix-Engine-Setup-*.exe", "Stereix-Engine-Portable-*.exe" -File
foreach ($exe in $LauncherInstallers) {
    $dst = Join-Path $OutputDir $exe.Name
    Copy-Item $exe.FullName $dst -Force
    $sizeMB = [math]::Round($exe.Length / 1048576, 2)
    Write-Host "  Copied Installer: $($exe.Name) ($sizeMB MB)"
}

# -------------------------------------------------------------------------
# 4. Generate SHA256SUMS.txt
# -------------------------------------------------------------------------
Write-Host ""
Write-Host ">>> [4/4] Computing SHA-256 Manifest..."
$ChecksumFile = Join-Path $OutputDir "SHA256SUMS.txt"
$ChecksumLines = @()

$ReleaseFiles = Get-ChildItem -Path $OutputDir -File | Where-Object { $_.Name -ne "SHA256SUMS.txt" }
foreach ($file in $ReleaseFiles) {
    $hash = (Get-FileHash -Algorithm SHA256 -Path $file.FullName).Hash.ToLowerInvariant()
    $line = "$hash  $($file.Name)"
    $ChecksumLines += $line
    Write-Host "  $line"
}

$ChecksumLines | Out-File -FilePath $ChecksumFile -Encoding utf8
Write-Host ""
Write-Host "=========================================================="
Write-Host " Release Packaging Complete! Output: $OutputDir"
Write-Host "=========================================================="
