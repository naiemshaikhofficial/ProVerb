@echo off
echo ========================================================
echo   Setting up C++ Compiler for ProducerToy ProVerb VST3
echo ========================================================
echo.
echo Launching Visual Studio C++ Installer...
echo Please click "Yes" when Windows asks for Administrator permission.
echo.

set INSTALLER_PATH="%LOCALAPPDATA%\Temp\WinGet\Microsoft.VisualStudio.2022.BuildTools.17.14.41\vs_BuildTools.exe"

if exist %INSTALLER_PATH% (
    start "" %INSTALLER_PATH% --passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
) else (
    winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" --accept-package-agreements --accept-source-agreements
)

echo.
echo Once the installation completes, run build.bat to compile ProVerb!
pause
