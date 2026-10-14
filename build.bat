@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo        PRODUCERTOY PROVERB VST3 - BUILD PIPELINE
echo ========================================================
echo.

set CMAKE_PATH="C:\Program Files\CMake\bin\cmake.exe"
if not exist %CMAKE_PATH% set CMAKE_PATH=cmake

echo [1/3] Configuring CMake project...
%CMAKE_PATH% -B build -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b %ERRORLEVEL%
)

echo [2/3] Compiling ProVerb VST3 (Release)...
%CMAKE_PATH% --build build --config Release --parallel
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed.
    pause
    exit /b %ERRORLEVEL%
)

if "%1"=="--deploy" (
    echo [3/3] Deploying to system VST3 directory...
    set VST3_DIR="%COMMONPROGRAMFILES%\VST3"
    if not exist %VST3_DIR% mkdir %VST3_DIR%
    if exist "build\ProVerb_artefacts\Release\VST3\ProVerb.vst3" (
        xcopy /E /I /Y "build\ProVerb_artefacts\Release\VST3\ProVerb.vst3" "%COMMONPROGRAMFILES%\VST3\ProVerb.vst3\"
        echo [DEPLOYED] Successfully copied ProVerb.vst3 to %COMMONPROGRAMFILES%\VST3\
    )
) else (
    echo [3/3] Build completed. Artifact ready at:
    echo       build\ProVerb_artefacts\Release\VST3\ProVerb.vst3
    echo       (To automatically copy to system VST3 folder, run: build.bat --deploy)
)

echo.
echo ========================================================
echo [SUCCESS] ProducerToy ProVerb VST3 is READY!
echo Run package_all_installers.bat to generate installer.
echo ========================================================
pause
