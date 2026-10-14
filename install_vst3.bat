@echo off
setlocal

echo ========================================================
echo Updating ProVerb VST3 in Common Files...
echo Please ensure FL Studio or your DAW is closed before running!
echo ========================================================
echo.

set "SRC_DIR=%~dp0build\ProVerb_artefacts\Release\VST3\ProVerb.vst3"
set "DST_DIR=C:\Program Files\Common Files\VST3\ProVerb.vst3"

if not exist "%SRC_DIR%" (
    echo [ERROR] Release VST3 bundle not found at:
    echo "%SRC_DIR%"
    echo Please compile ProVerb first!
    echo.
    pause
    exit /b 1
)

:: Check for Administrative Privileges
net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if "%1"=="-elevated" (
        echo [ERROR] Administrative rights could not be obtained.
        echo Please right-click on this script and select 'Run as administrator'.
        pause
        exit /b 1
    )
    echo [INFO] Administrative permissions required to update Common Files\VST3.
    echo [INFO] Requesting administrator elevation...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process cmd -ArgumentList '/c \"\"%~f0\"\" -elevated' -Verb RunAs"
    if %ERRORLEVEL% EQU 0 exit /b 0
    echo.
    echo [NOTICE] Elevation request was cancelled or unavailable.
    echo Please right-click on this file and choose 'Run as administrator'.
    pause
    exit /b 1
)

echo Copying VST3 bundle to "%DST_DIR%"...
xcopy /E /Y /I "%SRC_DIR%" "%DST_DIR%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] ProVerb VST3 updated successfully!
    echo Destination: "%DST_DIR%"
    echo You can now launch your DAW.
) else (
    echo.
    echo [ERROR] Copy failed. If a file is locked, please close FL Studio or
    echo         any running DAW and re-run this script.
)

if "%1"=="-elevated" (
    echo.
    echo Closing in 3 seconds...
    timeout /t 3 >nul
) else (
    echo.
    pause
)
