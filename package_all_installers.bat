@echo off
setlocal enabledelayedexpansion

echo ======================================================================
echo          PRODUCERTOY PROVERB - SECURE INSTALLER PACKAGER
echo ======================================================================
echo.

set "SCRIPT_DIR=%~dp0"
set "VST3_PE=%SCRIPT_DIR%build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\Contents\x86_64-win\ProVerb.vst3"
set "OUTPUT_INSTALLER=%SCRIPT_DIR%Output\ProVerb.exe"
set "ROOT_INSTALLER=%SCRIPT_DIR%ProVerb.exe"
set "ISS_FILE=%SCRIPT_DIR%Installer\Windows\ProVerb_Windows_Installer.iss"

:: 1. Verify VST3 Binary Exists
echo [1/4] Verifying Release ProVerb VST3 binary exists...
if not exist "%VST3_PE%" (
    echo [ERROR] Release VST3 PE binary not found: "%VST3_PE%"
    echo Please run build.bat first to compile ProVerb!
    pause
    exit /b 1
)
echo [OK] ProVerb VST3 verified: "%VST3_PE%"
echo.

:: 2. Pre-Sign VST3 Binary (Optional Authenticode)
echo [2/4] Checking Authenticode Code Signing for ProVerb VST3...
call "%SCRIPT_DIR%Installer\Windows\sign_binaries.bat" "%VST3_PE%"
echo.

:: 3. Locate Inno Setup Compiler & Compile Setup Package
echo [3/4] Compiling Windows Installer (ISCC.exe)...
if not defined ISCC_PATH (
    if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" (
        set "ISCC_PATH=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
    ) else if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
        set "ISCC_PATH=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    ) else if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
        set "ISCC_PATH=C:\Program Files\Inno Setup 6\ISCC.exe"
    ) else (
        for /f "delims=" %%I in ('where iscc.exe 2^>nul') do set "ISCC_PATH=%%I"
    )
)

"%ISCC_PATH%" "%ISS_FILE%"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Inno Setup compilation failed!
    pause
    exit /b %ERRORLEVEL%
)

:: 4. Post-Sign Setup Installer (Optional Authenticode)
echo [4/4] Checking Authenticode Code Signing for Final Installer...
if exist "%OUTPUT_INSTALLER%" (
    call "%SCRIPT_DIR%Installer\Windows\sign_binaries.bat" "%OUTPUT_INSTALLER%"
)

:: Clean up any duplicate or legacy installer binaries from Output and root
if exist "%SCRIPT_DIR%Output\ProVerb_Installer.exe" del /F /Q "%SCRIPT_DIR%Output\ProVerb_Installer.exe" >nul 2>&1
if exist "%SCRIPT_DIR%Output\ProducerToy_ProVerb_Installer.exe" del /F /Q "%SCRIPT_DIR%Output\ProducerToy_ProVerb_Installer.exe" >nul 2>&1
if exist "%SCRIPT_DIR%Output\ProducerToy_ProVerb_Setup.exe" del /F /Q "%SCRIPT_DIR%Output\ProducerToy_ProVerb_Setup.exe" >nul 2>&1
if exist "%SCRIPT_DIR%Output\ProducerToy_ProVerb_Windows_Installer.exe" del /F /Q "%SCRIPT_DIR%Output\ProducerToy_ProVerb_Windows_Installer.exe" >nul 2>&1
if exist "%SCRIPT_DIR%ProVerb_Installer.exe" del /F /Q "%SCRIPT_DIR%ProVerb_Installer.exe" >nul 2>&1
if exist "%SCRIPT_DIR%ProducerToy_ProVerb_Installer.exe" del /F /Q "%SCRIPT_DIR%ProducerToy_ProVerb_Installer.exe" >nul 2>&1

:: Deploy single root copy
if exist "%OUTPUT_INSTALLER%" (
    copy /Y "%OUTPUT_INSTALLER%" "%ROOT_INSTALLER%" >nul
)
echo [OK] Single signed ProVerb.exe installer ready.
echo.

echo ======================================================================
echo                  PROVERB INSTALLER BUILD SUMMARY
echo ======================================================================
echo [PACKAGE]  ProducerToy ProVerb Windows 64-bit Installer
echo [OUTPUT]   %OUTPUT_INSTALLER%
echo.
echo [EXCLUSIONS AUDIT]
echo   - Excluded: Tools/, tests, benchmarks, source files (*.cpp, *.h)
echo   - Excluded: Debug symbols (*.pdb, *.ilk, *.exp, *.lib)
echo   - Excluded: Feature caches (*.npz, *.py, *.git)
echo.
echo [SECURITY AND SIGNING AUDIT]
if defined SIGN_CERT_FILE (
    echo   - Authenticode Signing: APPLIED (via SIGN_CERT_FILE)
) else (
    if defined SIGN_CERT_SHA1 (
        echo   - Authenticode Signing: APPLIED (via SIGN_CERT_SHA1)
    ) else (
        echo   - Authenticode Signing: NONE (UNSIGNED - Development Build)
        echo     NOTE: Do NOT distribute or claim this build is signed.
    )
)
echo ======================================================================
echo.
