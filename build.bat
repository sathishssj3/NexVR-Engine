@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo Building NexVR Engine (Release x64)
echo =======================================================

:: Locate VsDevCmd.bat via vswhere if not already in developer environment
if not defined DevEnvDir (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "VS_INSTALL_DIR=%%i"
        )
    )
    if defined VS_INSTALL_DIR (
        echo Found Visual Studio at: !VS_INSTALL_DIR!
        call "!VS_INSTALL_DIR!\Common7\Tools\VsDevCmd.bat" -arch=x64
    ) else if exist "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=x64
    )
)

:: Configure if build folder does not exist
if not exist "build\CMakeCache.txt" (
    echo Configuring CMake project...
    cmake -B build -S . -A x64
    if errorlevel 1 exit /b 1
)

:: Build Release
cmake --build build --config Release
if errorlevel 1 (
    echo Build FAILED!
    exit /b 1
)

echo Build SUCCEEDED! Artifacts available in build\bin\
