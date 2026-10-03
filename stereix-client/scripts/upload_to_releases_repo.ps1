param (
    [string]$Token,
    [string]$Tag = "v0.1.99",
    [string]$Repo = "sathishssj3/Stereix-Engine-Releases",
    [string]$ArtifactsDir = ""
)

$ErrorActionPreference = "Stop"

if (-not $ArtifactsDir) {
    if (Test-Path "dist-release") {
        $ArtifactsDir = "dist-release"
    } elseif (Test-Path "..\dist-release") {
        $ArtifactsDir = "..\dist-release"
    } else {
        $ArtifactsDir = "launcher\dist-electron"
    }
}

if (-not $Token) {
    try {
        # Attempt local git credential fill if running locally
        $cred = "protocol=https`nhost=github.com`n" | git credential fill 2>$null
        foreach ($line in $cred) {
            if ($line -match "^password=(.+)$") {
                $Token = $matches[1].Trim()
            }
        }
    } catch {
        # Credential helper not configured or unavailable
    }
}

if (-not $Token) {
    Write-Warning "No GitHub token provided or found in credential helper. Pass -Token <PAT> to authenticate."
    Write-Host "Manual upload URL: https://github.com/$Repo/releases/new?tag=$Tag"
    exit 0
}

$headers = @{
    "Authorization" = "Bearer $Token"
    "Accept"        = "application/vnd.github.v3+json"
    "User-Agent"    = "Stereix-Release-Uploader"
}

Write-Host ">>> Querying releases for $Repo..."
$releases = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases" -Headers $headers -Method Get

$targetRelease = $releases | Where-Object { $_.tag_name -eq $Tag }
if (-not $targetRelease) {
    Write-Host ">>> Release with tag '$Tag' not found. Creating it..."
    $createBody = @{
        tag_name         = $Tag
        name             = "Stereix Engine $Tag"
        body             = "Automated release build of Stereix Engine ($Tag)."
        draft            = $false
        prerelease       = $false
    } | ConvertTo-Json

    $targetRelease = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases" -Headers $headers -Method Post -Body $createBody -ContentType "application/json"
    Write-Host ">>> Created release ID: $($targetRelease.id)"
} else {
    Write-Host ">>> Found existing release ID: $($targetRelease.id) (Tag: $($targetRelease.tag_name))"
}

$uploadBase = "https://uploads.github.com/repos/$Repo/releases/$($targetRelease.id)/assets"

# Locate setup, portable, SDK/standalone zip, and checksum manifest to upload
$filesToUpload = Get-ChildItem -Path "$ArtifactsDir\*" -Include "Stereix-Engine-Setup-*.exe", "Stereix-Engine-Portable-*.exe", "*.zip", "SHA256SUMS.txt" -File

if ($filesToUpload.Count -eq 0) {
    Write-Warning "No installer files found in $ArtifactsDir to upload."
    exit 0
}

# Fetch currently attached assets to replace if already present
$existingAssets = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/$($targetRelease.id)/assets" -Headers $headers -Method Get

foreach ($file in $filesToUpload) {
    $fileName = $file.Name
    $fileSizeMB = [math]::Round($file.Length / 1MB, 2)

    $existing = $existingAssets | Where-Object { $_.name -eq $fileName }
    if ($existing) {
        Write-Host ">>> Deleting existing asset '$fileName' (ID: $($existing.id))..."
        Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/assets/$($existing.id)" -Headers $headers -Method Delete
    }

    Write-Host ">>> Uploading $fileName ($fileSizeMB MB)..."
    $escapedName = [Uri]::EscapeDataString($fileName)
    $uploadUri = "${uploadBase}?name=${escapedName}"

    # L-12: Write authorization header to a transient curl config file to avoid exposing token on command line
    $curlCfg = [System.IO.Path]::GetTempFileName()
    try {
        [System.IO.File]::WriteAllLines($curlCfg, @("header = `"Authorization: Bearer $Token`""))
        $curlArgs = @(
            "-s",
            "-K", $curlCfg,
            "-X", "POST",
            "-H", "Content-Type: application/octet-stream",
            "--data-binary", "@$($file.FullName)",
            $uploadUri
        )
        & curl.exe @curlArgs | Out-Null
    } finally {
        if (Test-Path $curlCfg) { Remove-Item $curlCfg -Force }
    }
    Write-Host ">>> Upload complete for $fileName"
}

Write-Host ">>> All release assets uploaded successfully to https://github.com/$Repo/releases/tag/$Tag"
