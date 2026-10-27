<#
.SYNOPSIS
    Signs ProVerb VST3 binaries and Windows installer using Windows Authenticode (SignTool).

.DESCRIPTION
    Applies a SHA-256 Authenticode digital signature to ProVerb PE binaries and installers
    using a code-signing certificate installed in the CurrentUser\My certificate store
    or configured via environment variables.

    Supports both commercial OV/EV certificates and self-signed development certificates.
    
    Verifies every signature immediately after signing to ensure file integrity.

.PARAMETER Thumbprint
    SHA-1 thumbprint of the code-signing certificate in Cert:\CurrentUser\My.
    If omitted, checks $env:SIGN_CERT_SHA1 or auto-discovers "CN=ProducerToy ProVerb Development".

.PARAMETER Artifacts
    Specific file paths to sign. Defaults to the canonical Release VST3 PE binaries and Output installer.

.PARAMETER TimestampServer
    RFC 3161 timestamp server URL. Defaults to "http://timestamp.digicert.com".

.PARAMETER NoTimestamp
    Skip timestamping (useful for offline builds or when timestamping servers are unreachable).

.EXAMPLE
    .\sign_build.ps1
    .\sign_build.ps1 -NoTimestamp
    .\sign_build.ps1 -Thumbprint "14BFF694E3549D7DE1821A00B11189DEBA5E8628"
#>

[CmdletBinding()]
param(
    [string]$Thumbprint,
    [string[]]$Artifacts,
    [string]$TimestampServer = "http://timestamp.digicert.com",
    [switch]$NoTimestamp
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "        ProducerToy ProVerb - Authenticode Code Signing Engine" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

# 1. Locate SignTool.exe across standard Windows SDK paths and PATH
Write-Host "[1/4] Locating Windows SDK SignTool.exe..." -ForegroundColor Yellow

$signToolPath = $null
$candidatePaths = @(
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe",
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe",
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\signtool.exe",
    "C:\Program Files\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe",
    "C:\Program Files\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
)

foreach ($path in $candidatePaths) {
    if (Test-Path $path) {
        $signToolPath = $path
        break
    }
}

if (-not $signToolPath) {
    $cmd = Get-Command "signtool.exe" -ErrorAction SilentlyContinue
    if ($cmd) {
        $signToolPath = $cmd.Source
    }
}

if (-not $signToolPath) {
    Write-Host "[ERROR] signtool.exe was not found on this system." -ForegroundColor Red
    Write-Host "Please install the Windows 10/11 SDK or Visual Studio 'Desktop development with C++'." -ForegroundColor White
    exit 1
}

Write-Host "  Found SignTool: $signToolPath" -ForegroundColor Green
Write-Host ""

# 2. Resolve Signing Certificate Thumbprint
Write-Host "[2/4] Resolving Code Signing Certificate..." -ForegroundColor Yellow

if (-not $Thumbprint) {
    if ($env:SIGN_CERT_SHA1) {
        $Thumbprint = $env:SIGN_CERT_SHA1.Trim()
        Write-Host "  Using thumbprint from `$env:SIGN_CERT_SHA1" -ForegroundColor Gray
    }
}

if (-not $Thumbprint) {
    # Auto-discover ProducerToy development certificate
    $devCert = Get-ChildItem -Path "Cert:\CurrentUser\My" | Where-Object {
        $_.Subject -like "*ProducerToy ProVerb Development*" -and
        $_.NotAfter -gt (Get-Date) -and
        $_.HasPrivateKey -eq $true
    } | Select-Object -First 1

    if ($devCert) {
        $Thumbprint = $devCert.Thumbprint
        Write-Host "  Auto-detected local development certificate: $($devCert.Subject)" -ForegroundColor Gray
    }
}

if (-not $Thumbprint) {
    Write-Host "[ERROR] No code-signing certificate specified or discovered." -ForegroundColor Red
    Write-Host "To create a self-signed development certificate, run:" -ForegroundColor Yellow
    Write-Host "  .\Installer\Windows\create_dev_certificate.ps1" -ForegroundColor White
    Write-Host "Or specify -Thumbprint <SHA1_THUMBPRINT>" -ForegroundColor White
    exit 1
}

# Verify certificate exists in Cert:\CurrentUser\My
$cert = Get-Item -Path "Cert:\CurrentUser\My\$Thumbprint" -ErrorAction SilentlyContinue
if (-not $cert) {
    Write-Host "[ERROR] Certificate with thumbprint '$Thumbprint' not found in Cert:\CurrentUser\My." -ForegroundColor Red
    exit 1
}

if (-not $cert.HasPrivateKey) {
    Write-Host "[ERROR] Certificate '$Thumbprint' does not possess an accessible private key." -ForegroundColor Red
    exit 1
}

Write-Host "  Subject    : $($cert.Subject)" -ForegroundColor Green
Write-Host "  Thumbprint : $($cert.Thumbprint)" -ForegroundColor Green
Write-Host "  Expires    : $($cert.NotAfter)" -ForegroundColor Gray
Write-Host ""

# 3. Check Timestamp Server Connectivity (if not -NoTimestamp)
$useTimestamp = $false
if (-not $NoTimestamp) {
    Write-Host "[3/4] Checking RFC 3161 Timestamp Server connectivity..." -ForegroundColor Yellow
    try {
        $request = [System.Net.WebRequest]::Create($TimestampServer)
        $request.Timeout = 3000
        $request.Method = "HEAD"
        $response = $request.GetResponse()
        $response.Close()
        $useTimestamp = $true
        Write-Host "  Timestamp server reachable: $TimestampServer" -ForegroundColor Green
    }
    catch {
        Write-Host "  [NOTICE] Timestamp server ($TimestampServer) unreachable or offline." -ForegroundColor DarkYellow
        Write-Host "  Proceeding with local signing without timestamping." -ForegroundColor DarkYellow
        $useTimestamp = $false
    }
} else {
    Write-Host "[3/4] Timestamping disabled via -NoTimestamp." -ForegroundColor Gray
}
Write-Host ""

# 4. Resolve Target Artifacts
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Resolve-Path (Join-Path $scriptDir "..\..")

if (-not $Artifacts -or $Artifacts.Count -eq 0) {
    $Artifacts = @(
        (Join-Path $projectRoot "build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\Contents\x86_64-win\ProVerb.vst3"),
        (Join-Path $projectRoot "build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\Contents\x86-win\ProVerb.vst3"),
        (Join-Path $projectRoot "Output\ProVerb.exe"),
        (Join-Path $projectRoot "ProVerb.exe"),
        (Join-Path $projectRoot "Output\ProVerb_Installer.exe"),
        (Join-Path $projectRoot "ProVerb_Installer.exe")
    )
}

$foundArtifacts = @()
foreach ($target in $Artifacts) {
    if (Test-Path $target) {
        $foundArtifacts += (Resolve-Path $target).Path
    }
}

if ($foundArtifacts.Count -eq 0) {
    Write-Host "[ERROR] None of the expected build artifacts exist on disk to sign." -ForegroundColor Red
    Write-Host "Expected targets:" -ForegroundColor Gray
    foreach ($t in $Artifacts) { Write-Host "  - $t" -ForegroundColor Gray }
    Write-Host "Please build the plugin and installer first!" -ForegroundColor Yellow
    exit 1
}

Write-Host "[4/4] Signing and Verifying Artifacts..." -ForegroundColor Yellow
$signedCount = 0
$failedCount = 0

foreach ($filePath in $foundArtifacts) {
    $fileName = Split-Path -Leaf $filePath
    Write-Host "  -> Signing: $fileName" -ForegroundColor Cyan
    Write-Host "     Path   : $filePath" -ForegroundColor DarkGray

    # Construct SignTool arguments
    $signArgs = @(
        "sign",
        "/sha1", $Thumbprint,
        "/s", "My",
        "/fd", "SHA256"
    )

    if ($useTimestamp) {
        $signArgs += @("/tr", $TimestampServer, "/td", "SHA256")
    }

    $signArgs += $filePath

    # Execute SignTool
    $signOutput = & $signToolPath @signArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "     [ERROR] Signing failed with exit code $LASTEXITCODE" -ForegroundColor Red
        Write-Host "     SignTool output: $signOutput" -ForegroundColor Red
        $failedCount++
        continue
    }

    # Verify signature integrity via PowerShell Get-AuthenticodeSignature
    $sig = Get-AuthenticodeSignature -FilePath $filePath
    if ($sig.Status -eq 'NotSigned') {
        Write-Host "     [ERROR] File remains unsigned after SignTool execution." -ForegroundColor Red
        $failedCount++
        continue
    }
    elseif ($sig.Status -eq 'HashMismatch') {
        Write-Host "     [ERROR] Signature hash mismatch: binary was altered or corrupted." -ForegroundColor Red
        $failedCount++
        continue
    }

    $hash = (Get-FileHash -Path $filePath -Algorithm SHA256).Hash
    Write-Host "     [SUCCESS] Authenticode signature applied and integrity verified!" -ForegroundColor Green
    Write-Host "     SHA-256 : $hash" -ForegroundColor Gray
    Write-Host "     Status  : $($sig.Status) ($($sig.StatusMessage))" -ForegroundColor Gray
    $signedCount++
}

Write-Host ""
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "                      SIGNING EXECUTION SUMMARY" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "  Total Artifacts Targeted : $($foundArtifacts.Count)" -ForegroundColor White
Write-Host "  Successfully Signed      : $signedCount" -ForegroundColor Green
Write-Host "  Failed Signatures        : $failedCount" -ForegroundColor $(if ($failedCount -eq 0) { 'Gray' } else { 'Red' })
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

if ($failedCount -gt 0) {
    exit 1
}

exit 0
