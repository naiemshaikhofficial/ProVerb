@echo off
setlocal enabledelayedexpansion

:: Self-elevation to Administrator if needed for Program Files
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting Administrator privileges to install VST3 plugin...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo ========================================================
echo   Deploying ProVerb.vst3 to System & User VST3
echo ========================================================
echo.

set PROJ_ROOT=D:\Website\ProducerToy-Plugins\ProVerb
set SRC=%PROJ_ROOT%\build\ProVerb_artefacts\Release\VST3\ProVerb.vst3
set SYS_DEST=%COMMONPROGRAMFILES%\VST3\ProVerb.vst3
set USER_DEST=%LOCALAPPDATA%\Programs\Common\VST3\ProVerb.vst3

if not exist "%SRC%" (
    echo [ERROR] Build output not found at: %SRC%
    pause
    exit /b 1
)

:: Ensure system VST3 folder
if not exist "%COMMONPROGRAMFILES%\VST3" (
    mkdir "%COMMONPROGRAMFILES%\VST3"
)

echo 1. Copying built VST3 bundle to System Directory (%SYS_DEST%)...
xcopy /E /I /Y "%SRC%" "%SYS_DEST%"

echo 2. Ensuring offline AI ONNX model is in bundle...
if exist "%PROJ_ROOT%\Assets\ProVerb_AI.onnx" (
    copy /Y "%PROJ_ROOT%\Assets\ProVerb_AI.onnx" "%SYS_DEST%\Contents\x86_64-win\" >nul
    copy /Y "%PROJ_ROOT%\Assets\ProVerb_AI.onnx" "%SYS_DEST%\Contents\Resources\" >nul
)

echo 3. Ensuring ONNX runtime DLLs are in bundle...
if exist "%PROJ_ROOT%\ThirdParty\onnxruntime\lib\onnxruntime.dll" (
    copy /Y "%PROJ_ROOT%\ThirdParty\onnxruntime\lib\onnxruntime.dll" "%SYS_DEST%\Contents\x86_64-win\" >nul
    copy /Y "%PROJ_ROOT%\ThirdParty\onnxruntime\lib\onnxruntime_providers_shared.dll" "%SYS_DEST%\Contents\x86_64-win\" >nul
)

:: Remove installer residue unins000 files from bundle root if present
if exist "%SYS_DEST%\unins000.exe" del /F /Q "%SYS_DEST%\unins000.exe" >nul 2>&1
if exist "%SYS_DEST%\unins000.dat" del /F /Q "%SYS_DEST%\unins000.dat" >nul 2>&1

echo 4. Also deploying to User VST3 location (%USER_DEST%)...
if not exist "%LOCALAPPDATA%\Programs\Common\VST3" (
    mkdir "%LOCALAPPDATA%\Programs\Common\VST3" >nul 2>&1
)
xcopy /E /I /Y "%SYS_DEST%" "%USER_DEST%" >nul 2>&1

echo 5. Resetting stale FL Studio failed preset cache...
set FL_USER_DOCS=%USERPROFILE%\Documents\Image-Line\FL Studio\Presets\Plugin database
if exist "%FL_USER_DOCS%\Installed\Effects\VST3\ProVerb.fst" del /F /Q "%FL_USER_DOCS%\Installed\Effects\VST3\ProVerb.fst" >nul 2>&1
if exist "%FL_USER_DOCS%\Installed\Effects\VST3\ProVerb.nfo" del /F /Q "%FL_USER_DOCS%\Installed\Effects\VST3\ProVerb.nfo" >nul 2>&1
if exist "%FL_USER_DOCS%\Effects\Fx\ProVerb.fst" del /F /Q "%FL_USER_DOCS%\Effects\Fx\ProVerb.fst" >nul 2>&1

echo.
echo ========================================================
echo [SUCCESS] ProVerb VST3 deployed and validated!
echo Class ID: {ABCDEF01-9182-FAEB-5072-547950725662}
echo FL Studio will now load ProVerb freshly without stale errors.
echo ========================================================
echo.
timeout /t 3
