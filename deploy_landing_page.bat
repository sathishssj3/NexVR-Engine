@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   Stereix Engine -- Cloudflare Landing Page Deployer
echo ========================================================
echo.

cd /d "%~dp0stereix-docs\landing-page"

echo [*] Deploying to primary production: https://stereix-engine.pages.dev ...
call npx wrangler pages deploy public --project-name=stereix-engine --branch=main --commit-dirty=true
if %errorlevel% neq 0 (
    echo [ERROR] Deployment to stereix-engine failed.
)

echo.
echo [*] Deploying to secondary mirror: https://nexvr-engine.pages.dev ...
call npx wrangler pages deploy public --project-name=nexvr-engine --branch=main --commit-dirty=true
if %errorlevel% neq 0 (
    echo [WARNING] Secondary mirror deployment encountered an issue.
)

echo.
echo ========================================================
echo   Deployment Complete!
echo   Primary:   https://stereix-engine.pages.dev
echo   Secondary: https://nexvr-engine.pages.dev
echo ========================================================
echo.
pause
