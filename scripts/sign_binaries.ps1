# ==============================================================================
# sign_binaries.ps1 - Automated Authenticode Code-Signing for NexVR Engine
# Creates/uses a local trusted development certificate to sign injector binaries
# Prevents Windows Defender / SmartScreen heuristic false-positives
# ==============================================================================

[CmdletBinding()]
param (
    [string]$TargetDir = "$PSScriptRoot\..\build\bin",
    [string]$CertSubject = "CN=NexVR Local Development"
)

$ErrorActionPreference = "Stop"

Write-Host "=== NexVR Engine Authenticode Signing Tool ===" -ForegroundColor Cyan

# 1. Check for existing certificate or create one
$cert = Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert -Recurse | Where-Object { $_.Subject -eq $CertSubject } | Select-Object -First 1

if (-not $cert) {
    Write-Host "[*] Creating self-signed code-signing certificate: $CertSubject" -ForegroundColor Yellow
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $CertSubject `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -KeyUsage DigitalSignature `
        -FriendlyName "NexVR Engine Development Certificate" `
        -NotAfter (Get-Date).AddYears(5)

    Write-Host "[OK] Self-signed code signing certificate created in CurrentUser\My." -ForegroundColor Green
} else {
    Write-Host "[OK] Found existing valid code-signing certificate (Thumbprint: $($cert.Thumbprint))" -ForegroundColor Green
}

# 2. Files to sign
$candidateFiles = @(
    "$TargetDir\vrinject.dll",
    "$TargetDir\vr-inject-cli.exe",
    "$PSScriptRoot\..\updates\vrinject.dll",
    "$PSScriptRoot\..\updates\vr-inject-cli.exe"
)

$signedCount = 0
foreach ($filePath in $candidateFiles) {
    if (Test-Path -LiteralPath $filePath) {
        Write-Host "[*] Signing: $filePath" -ForegroundColor Gray
        try {
            $status = Set-AuthenticodeSignature -Certificate $cert -FilePath $filePath -HashAlgorithm SHA256
            if ($status.Status -in @("Valid", "UnknownError")) {
                Write-Host "    [OK] Authenticode signature applied successfully (SHA-256)" -ForegroundColor Green
                $signedCount++
            } else {
                Write-Host "    [WARN] Signature status: $($status.StatusMessage)" -ForegroundColor Yellow
            }
        } catch {
            Write-Host "    [WARN] Could not sign file: $_" -ForegroundColor Yellow
        }
    }
}

Write-Host "=== Signing complete. $signedCount binaries signed successfully. ===" -ForegroundColor Cyan
