$ErrorActionPreference = 'Stop'

$SourceDir = "C:\Users\sathi\.gemini\antigravity\scratch\vr-inject\build\bin"
$ZipName = "NexVR_Beta_v1.0.zip"
$ZipPath = "C:\Users\sathi\.gemini\antigravity\scratch\vr-inject\$ZipName"
$TempDir = "C:\Users\sathi\.gemini\antigravity\scratch\vr-inject\BetaPackageTemp"

Write-Host "Creating temporary packaging directory..."
if (Test-Path $TempDir) { Remove-Item $TempDir -Recurse -Force }
New-Item -ItemType Directory -Path $TempDir | Out-Null

$FilesToCopy = @(
    "vrinject.dll",
    "vr-inject-cli.exe",
    "onnxruntime.dll",
    "README.md",
    "config.json"
)

Write-Host "Copying files to temporary directory..."
foreach ($File in $FilesToCopy) {
    $SourcePath = Join-Path $SourceDir $File
    if (Test-Path $SourcePath) {
        Copy-Item -Path $SourcePath -Destination $TempDir
        Write-Host "  Copied: $File"
    } else {
        Write-Warning "  Missing: $File"
    }
}

if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }

Write-Host "Compressing to $ZipName..."
Compress-Archive -Path "$TempDir\*" -DestinationPath $ZipPath -Force

Write-Host "Cleaning up temporary directory..."
Remove-Item $TempDir -Recurse -Force

Write-Host "Done! Package created at: $ZipPath"
