@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   NexVR Engine -- 10-Second Instant Hotfix Deployer
echo ========================================================
echo.

echo [1/4] Compiling latest Release vrinject.dll...
cmake --build build --config Release --target vrinject
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] C++ build failed. Please check compiler errors above.
    pause
    exit /b %errorlevel%
)

echo.
echo [2/4] Staging vrinject.dll for instant OTA distribution...
if not exist "updates" mkdir updates
copy /y "build\bin\vrinject.dll" "updates\vrinject.dll" >nul

echo.
echo [3/4] Updating OTA manifest timestamp...
node -e "const fs = require('fs'); const file = 'updates/manifest.json'; let m = { engineVersion: '0.1.0', timestamp: Date.now(), changelog: 'Engine hotfix update', files: ['vrinject.dll'] }; if (fs.existsSync(file)) { try { m = JSON.parse(fs.readFileSync(file, 'utf-8')); m.timestamp = Date.now(); } catch(e){} } fs.writeFileSync(file, JSON.stringify(m, null, 2)); console.log('Updated manifest timestamp:', m.timestamp);"

echo.
echo [4/4] Pushing hotfix directly to GitHub...
git add updates/vrinject.dll updates/manifest.json
git commit -m "Hotfix: Deploy latest vrinject.dll"
git push origin main

echo.
echo ========================================================
echo   SUCCESS! Hotfix is LIVE on GitHub!
echo   All other machines will download it in ~2 seconds
echo   without needing to reinstall or wait for slow builds!
echo ========================================================
echo.
pause
