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
echo [3/4] Recomputing SHA-256 cryptographic hashes and syncing assets...
node scripts/sync_assets.js

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
