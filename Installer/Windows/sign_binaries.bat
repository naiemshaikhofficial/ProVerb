@echo off
setlocal EnableDelayedExpansion

:: ============================================================================
:: ProVerb - Authenticode Signing Utility
:: Security Policy:
:: 1. NEVER store private keys, PFX, or certificates in the repository.
:: 2. Credentials must only be provided via secure CI/CD or environment variables.
:: 3. If no certificate is present, clearly report the binary as UNSIGNED.
:: ============================================================================

set "TARGET_FILE=%~1"
if "%TARGET_FILE%"=="" (
    echo [ERROR] No target file specified for signing.
    exit /b 1
)

if not exist "%TARGET_FILE%" (
    echo [ERROR] Target file does not exist: "%TARGET_FILE%"
    exit /b 1
)

:: Locate signtool.exe across standard Windows SDK paths or PATH
set "SIGNTOOL_EXE="
if exist "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" (
    set "SIGNTOOL_EXE=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
) else if exist "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe" (
    set "SIGNTOOL_EXE=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
) else if exist "C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\signtool.exe" (
    set "SIGNTOOL_EXE=C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\signtool.exe"
) else (
    for /f "delims=" %%I in ('where signtool.exe 2^>nul') do (
        set "SIGNTOOL_EXE=%%I"
    )
)

:: Timestamp server configuration (RFC 3161 compliant)
set "TIMESTAMP_URL=%SIGN_TIMESTAMP_URL%"
if "%TIMESTAMP_URL%"=="" set "TIMESTAMP_URL=http://timestamp.digicert.com"

:: Mode 1: File-based certificate (PFX) via SIGN_CERT_FILE
if defined SIGN_CERT_FILE (
    if not exist "%SIGN_CERT_FILE%" (
        echo [ERROR] SIGN_CERT_FILE specified but not found on disk: "%SIGN_CERT_FILE%"
        goto report_unsigned
    )
    if "%SIGNTOOL_EXE%"=="" (
        echo [ERROR] signtool.exe could not be located on this machine.
        goto report_unsigned
    )

    echo [SIGNTOOL] Signing "%TARGET_FILE%" using PFX certificate...
    if defined SIGN_CERT_PASSWORD (
        "%SIGNTOOL_EXE%" sign /f "%SIGN_CERT_FILE%" /p "%SIGN_CERT_PASSWORD%" /fd SHA256 /tr "%TIMESTAMP_URL%" /td SHA256 "%TARGET_FILE%"
    ) else (
        "%SIGNTOOL_EXE%" sign /f "%SIGN_CERT_FILE%" /fd SHA256 /tr "%TIMESTAMP_URL%" /td SHA256 "%TARGET_FILE%"
    )

    if !ERRORLEVEL! EQU 0 (
        echo [VERIFY] Verifying Authenticode signature...
        "%SIGNTOOL_EXE%" verify /pa /q "%TARGET_FILE%"
        if !ERRORLEVEL! EQU 0 (
            echo [SUCCESS] Binary Authenticode-signed and verified: "%TARGET_FILE%"
            exit /b 0
        ) else (
            echo [WARNING] Signature verification check failed.
            exit /b !ERRORLEVEL!
        )
    ) else (
        echo [WARNING] signtool execution returned error code !ERRORLEVEL!.
        goto report_unsigned
    )
)

:: Mode 2: Windows Certificate Store thumbprint via SIGN_CERT_SHA1
if defined SIGN_CERT_SHA1 (
    if "%SIGNTOOL_EXE%"=="" (
        echo [ERROR] signtool.exe could not be located on this machine.
        goto report_unsigned
    )

    echo [SIGNTOOL] Signing "%TARGET_FILE%" using Certificate Store SHA1 thumbprint...
    "%SIGNTOOL_EXE%" sign /sha1 "%SIGN_CERT_SHA1%" /fd SHA256 /tr "%TIMESTAMP_URL%" /td SHA256 "%TARGET_FILE%"

    if !ERRORLEVEL! EQU 0 (
        echo [VERIFY] Verifying Authenticode signature...
        "%SIGNTOOL_EXE%" verify /pa /q "%TARGET_FILE%"
        if !ERRORLEVEL! EQU 0 (
            echo [SUCCESS] Binary Authenticode-signed and verified: "%TARGET_FILE%"
            exit /b 0
        ) else (
            echo [WARNING] Signature verification check failed.
            exit /b !ERRORLEVEL!
        )
    ) else (
        echo [WARNING] signtool execution returned error code !ERRORLEVEL!.
        goto report_unsigned
    )
)

:report_unsigned
echo ----------------------------------------------------------------------
echo [SECURITY NOTICE: AUTHENTICODE SIGNING OMITTED]
echo File   : "%TARGET_FILE%"
echo Status : UNSIGNED (Local Development / Evaluation Build)
echo Reason : No Authenticode credentials provided in environment.
echo          To enable signing, set SIGN_CERT_FILE (and optionally
echo          SIGN_CERT_PASSWORD) or SIGN_CERT_SHA1 in your environment.
echo POLICY : This build must NOT be advertised or claimed as signed.
echo ----------------------------------------------------------------------
exit /b 0
