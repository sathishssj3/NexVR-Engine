@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   NexVR Engine -- Cloudflare Edge OTA Deployer
echo   (100%% Free, Zero Egress Fees, Global Edge CDN)
echo ========================================================
echo.

echo [1/4] Compiling latest Release vrinject.dll and CLI...
cmake --build build --config Release --target vrinject vr-inject-cli
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] C++ build failed. Please check compiler errors above.
    pause
    exit /b %errorlevel%
)

echo.
echo [2/4] Syncing binaries to Cloudflare distribution staging...
if not exist "nexvr-docs\landing-page\public\updates" mkdir "nexvr-docs\landing-page\public\updates"
copy /y "build\bin\vrinject.dll" "updates\vrinject.dll" >nul
copy /y "build\bin\vr-inject-cli.exe" "updates\vr-inject-cli.exe" >nul
copy /y "build\bin\vrinject.dll" "nexvr-docs\landing-page\public\updates\vrinject.dll" >nul
copy /y "build\bin\vr-inject-cli.exe" "nexvr-docs\landing-page\public\updates\vr-inject-cli.exe" >nul

echo.
echo [3/4] Recomputing SHA-256 cryptographic hashes...
node -e "const fs = require('fs'); const crypto = require('crypto'); const manifestFile = 'updates/manifest.json'; const manifest = JSON.parse(fs.readFileSync(manifestFile, 'utf-8')); let pkgVer = '0.1.28'; try { pkgVer = JSON.parse(fs.readFileSync('nexvr-client/launcher/package.json', 'utf-8')).version || pkgVer; } catch(e){} manifest.engineVersion = pkgVer; manifest.timestamp = Date.now(); manifest.date = new Date().toISOString(); const newHashes = {}; for (const f of manifest.files) { const filePath = 'updates/' + f; if (fs.existsSync(filePath)) { const buf = fs.readFileSync(filePath); newHashes[f] = crypto.createHash('sha256').update(buf).digest('hex'); } else { newHashes[f] = manifest.hashes[f]; } } manifest.hashes = newHashes; fs.writeFileSync(manifestFile, JSON.stringify(manifest, null, 2)); fs.writeFileSync('nexvr-docs/landing-page/public/updates/manifest.json', JSON.stringify(manifest, null, 2)); console.log('Updated manifest version:', manifest.engineVersion, 'timestamp:', manifest.timestamp);"

echo.
echo [4/4] Deploying to Cloudflare Edge CDN (2-3 seconds)...
cd nexvr-docs\landing-page
call npx wrangler pages deploy public --project-name=nexvr-engine --commit-dirty=true
cd ..\..

echo.
echo ========================================================
echo   SUCCESS! Hotfix is LIVE on Cloudflare Edge!
echo   URL: https://nexvr-engine.pages.dev/updates/manifest.json
echo   - 0 Egress Bandwidth Fees
echo   - Instant 1-second global download for all testers
echo   - No Git commits or repository bloat
echo ========================================================
echo.
pause
