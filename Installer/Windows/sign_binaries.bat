@echo off
setlocal EnableDelayedExpansion

REM ============================================================================
REM ProVerb - Authenticode Signing Utility
REM Security Policy:
REM 1. NEVER store private keys, PFX, or certificates in the repository.
REM 2. Credentials must only be provided via secure CI/CD or environment variables.
REM 3. If no certificate is present, clearly report the binary as UNSIGNED.
REM ============================================================================

set "TARGET_FILE=%~1"
if "%TARGET_FILE%"=="" (
    echo [ERROR] No target file specified for signing.
    exit /b 1
)

if not exist "%TARGET_FILE%" (
    echo [ERROR] Target file does not exist: "%TARGET_FILE%"
    exit /b 1
)

REM Mode 1: File-based certificate (PFX) via SIGN_CERT_FILE
if defined SIGN_CERT_FILE (
    if not exist "%SIGN_CERT_FILE%" (
        echo [ERROR] SIGN_CERT_FILE specified but not found on disk: "%SIGN_CERT_FILE%"
        goto report_unsigned
    )
    
    set "SIGNTOOL_EXE="
    if exist "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" (
        set "SIGNTOOL_EXE=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
    ) else if exist "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe" (
        set "SIGNTOOL_EXE=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
    ) else if exist "C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\signtool.exe" (
        set "SIGNTOOL_EXE=C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\signtool.exe"
    ) else (
        for /f "delims=" %%I in ('where signtool.exe 2^>nul') do set "SIGNTOOL_EXE=%%I"
    )

    if "%SIGNTOOL_EXE%"=="" (
        echo [ERROR] signtool.exe could not be located on this machine.
        goto report_unsigned
    )

    set "TIMESTAMP_URL=%SIGN_TIMESTAMP_URL%"
    if "%TIMESTAMP_URL%"=="" set "TIMESTAMP_URL=http://timestamp.digicert.com"

    echo [SIGNTOOL] Signing "%TARGET_FILE%" using PFX certificate...
    if defined SIGN_CERT_PASSWORD (
        "%SIGNTOOL_EXE%" sign /f "%SIGN_CERT_FILE%" /p "%SIGN_CERT_PASSWORD%" /fd SHA256 /tr "%TIMESTAMP_URL%" /td SHA256 "%TARGET_FILE%"
    ) else (
        "%SIGNTOOL_EXE%" sign /f "%SIGN_CERT_FILE%" /fd SHA256 /tr "%TIMESTAMP_URL%" /td SHA256 "%TARGET_FILE%"
    )

    if !ERRORLEVEL! EQU 0 (
        powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify_signatures.ps1" -Artifacts "%TARGET_FILE%"
        exit /b !ERRORLEVEL!
    )
    goto report_unsigned
)

REM Mode 2: Delegate to PowerShell sign_build.ps1 (Current User store / SIGN_CERT_SHA1 / Dev cert)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0sign_build.ps1" -Artifacts "%TARGET_FILE%"
if %ERRORLEVEL% EQU 0 (
    exit /b 0
)

:report_unsigned
echo ----------------------------------------------------------------------
echo [SECURITY NOTICE: AUTHENTICODE SIGNING OMITTED]
echo File   : "%TARGET_FILE%"
echo Status : UNSIGNED (Local Development / Evaluation Build)
echo Reason : No Authenticode credentials provided in environment.
echo          To enable signing, run:
echo            powershell -ExecutionPolicy Bypass -File .\Installer\Windows\create_dev_certificate.ps1
echo          or set SIGN_CERT_FILE / SIGN_CERT_SHA1 in your environment.
echo POLICY : This build must NOT be advertised or claimed as signed.
echo ----------------------------------------------------------------------
exit /b 0
