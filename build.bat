@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo        PRODUCERTOY PROVERB VST3 - BUILD PIPELINE
echo ========================================================
echo.

set CMAKE_PATH="C:\Program Files\CMake\bin\cmake.exe"
if not exist %CMAKE_PATH% set CMAKE_PATH=cmake

echo [1/4] Configuring and Compiling 64-bit ProVerb VST3 (Release)...
%CMAKE_PATH% -B build -S . -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 64-bit CMake configuration failed.
    pause
    exit /b %ERRORLEVEL%
)
%CMAKE_PATH% --build build --config Release --target ProVerb_VST3 --parallel
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 64-bit compilation failed.
    pause
    exit /b %ERRORLEVEL%
)

echo [2/4] Configuring and Compiling 32-bit (x86) ProVerb VST3 (Release)...
%CMAKE_PATH% -B build32 -S . -G "Visual Studio 17 2022" -A Win32
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 32-bit CMake configuration failed.
    pause
    exit /b %ERRORLEVEL%
)
%CMAKE_PATH% --build build32 --config Release --target ProVerb_VST3 --parallel
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 32-bit compilation failed.
    pause
    exit /b %ERRORLEVEL%
)

echo [3/4] Assembling Universal Dual-Architecture VST3 Bundle (32-bit + 64-bit)...
if exist "build32\ProVerb_artefacts\Release\VST3\ProVerb.vst3\Contents\x86-win" (
    xcopy /E /I /Y "build32\ProVerb_artefacts\Release\VST3\ProVerb.vst3\Contents\x86-win" "build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\Contents\x86-win\" >nul
    echo [OK] Merged x86-win (32-bit) and x86_64-win (64-bit) into unified bundle!
)

if "%1"=="--deploy" (
    echo [4/4] Deploying to system VST3 directory...
    set VST3_DIR="%COMMONPROGRAMFILES%\VST3"
    if not exist %VST3_DIR% mkdir %VST3_DIR%
    if exist "build\ProVerb_artefacts\Release\VST3\ProVerb.vst3" (
        xcopy /E /I /Y "build\ProVerb_artefacts\Release\VST3\ProVerb.vst3" "%COMMONPROGRAMFILES%\VST3\ProVerb.vst3\"
        echo [DEPLOYED] Successfully copied universal ProVerb.vst3 to %COMMONPROGRAMFILES%\VST3\
    )
) else (
    echo [4/4] Build completed. Universal Dual-Architecture Artifact ready at:
    echo       build\ProVerb_artefacts\Release\VST3\ProVerb.vst3
    echo       (To automatically copy to system VST3 folder, run: build.bat --deploy)
)

echo.
echo ========================================================
echo [SUCCESS] ProducerToy ProVerb Universal VST3 (32 & 64-bit) is READY!
echo Run package_all_installers.bat to generate installer.
echo ========================================================
pause
