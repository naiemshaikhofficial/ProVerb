<#
.SYNOPSIS
    Creates an optional, zero-cost self-signed development code-signing certificate for ProVerb.

.DESCRIPTION
    Generates a private-key-backed X.509 certificate in the current user's personal certificate
    store (Cert:\CurrentUser\My) with the Code Signing Enhanced Key Usage (EKU: 1.3.6.1.5.5.7.3.3).
    
    This certificate enables local Authenticode signing and binary integrity verification with SignTool
    without purchasing a commercial code-signing certificate.

    IMPORTANT SECURITY NOTICE:
    This certificate is SELF-SIGNED and FOR DEVELOPMENT USE ONLY.
    It is NOT trusted by other computers and will NOT eliminate Windows SmartScreen or "Unknown Publisher"
    warnings on customer PCs. For commercial distribution, a public OV/EV certificate from a trusted CA
    (e.g., DigiCert, Sectigo) is required.

.PARAMETER Subject
    The distinguished subject name for the certificate. Defaults to "CN=ProducerToy ProVerb Development".

.PARAMETER ValidYears
    Validity duration in years. Defaults to 3 years.

.PARAMETER Force
    If specified, creates a new certificate even if a valid one already exists with the same Subject.

.OUTPUTS
    The SHA-1 thumbprint of the existing or newly created certificate.

.EXAMPLE
    .\create_dev_certificate.ps1
    .\create_dev_certificate.ps1 -Force
#>

[CmdletBinding()]
param(
    [string]$Subject = "CN=ProducerToy ProVerb Development",
    [int]$ValidYears = 3,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host " ProducerToy ProVerb - Development Code Signing Certificate Setup" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

# 1. Idempotency Check: Look for an existing, unexpired certificate in CurrentUser\My
Write-Host "[1/3] Searching for existing development certificate..." -ForegroundColor Yellow
$existingCert = Get-ChildItem -Path "Cert:\CurrentUser\My" | Where-Object {
    $_.Subject -eq $Subject -and
    $_.NotAfter -gt (Get-Date) -and
    $_.HasPrivateKey -eq $true -and
    ($_.EnhancedKeyUsageList | Where-Object { $_.ObjectId -eq '1.3.6.1.5.5.7.3.3' -or $_.FriendlyName -eq 'Code Signing' })
} | Sort-Object -Property NotAfter -Descending | Select-Object -First 1

if ($existingCert -and -not $Force) {
    Write-Host "[IDEMPOTENT] A valid development code-signing certificate already exists." -ForegroundColor Green
    Write-Host ""
    Write-Host "  Subject     : $($existingCert.Subject)" -ForegroundColor Gray
    Write-Host "  Thumbprint  : $($existingCert.Thumbprint)" -ForegroundColor Green
    Write-Host "  Valid From  : $($existingCert.NotBefore)" -ForegroundColor Gray
    Write-Host "  Expires On  : $($existingCert.NotAfter)" -ForegroundColor Gray
    Write-Host "  Store       : Cert:\CurrentUser\My (Personal)" -ForegroundColor Gray
    Write-Host "  Algorithm   : $($existingCert.SignatureAlgorithm.FriendlyName)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "----------------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "STATUS : READY FOR LOCAL SIGNING" -ForegroundColor Green
    Write-Host "TYPE   : SELF-SIGNED / DEVELOPMENT ONLY (Not for public distribution)" -ForegroundColor Yellow
    Write-Host "----------------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "To use this certificate with build scripts, set:" -ForegroundColor Cyan
    Write-Host "  `$env:SIGN_CERT_SHA1 = `"$($existingCert.Thumbprint)`"" -ForegroundColor White
    Write-Host ""
    return $existingCert.Thumbprint
}

# 2. Certificate Generation
Write-Host "[2/3] Generating new self-signed Code Signing certificate..." -ForegroundColor Yellow
try {
    $newCert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $Subject `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -HashAlgorithm SHA256 `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -NotAfter (Get-Date).AddYears($ValidYears) `
        -FriendlyName "ProducerToy ProVerb Development Code Signing"

    Write-Host "[SUCCESS] Created development code-signing certificate!" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Subject     : $($newCert.Subject)" -ForegroundColor Gray
    Write-Host "  Thumbprint  : $($newCert.Thumbprint)" -ForegroundColor Green
    Write-Host "  Valid From  : $($newCert.NotBefore)" -ForegroundColor Gray
    Write-Host "  Expires On  : $($newCert.NotAfter)" -ForegroundColor Gray
    Write-Host "  Store       : Cert:\CurrentUser\My (Personal)" -ForegroundColor Gray
    Write-Host ""
}
catch {
    Write-Error "Failed to generate self-signed certificate: $_"
    exit 1
}

# 3. Security Guidelines & Usage Information
Write-Host "[3/3] Security & Trust Information" -ForegroundColor Yellow
Write-Host "----------------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "IMPORTANT CONSTRAINTS & BEST PRACTICES:" -ForegroundColor Yellow
Write-Host " 1. This certificate is stored in CurrentUser\My and does NOT touch" -ForegroundColor Gray
Write-Host "    Local Machine Trusted Root or Trusted Publishers." -ForegroundColor Gray
Write-Host " 2. Windows will report this certificate as 'Untrusted Root' by default." -ForegroundColor Gray
Write-Host "    This is NORMAL and EXPECTED for self-signed certificates." -ForegroundColor Gray
Write-Host " 3. File integrity and Authenticode signature verification WILL work," -ForegroundColor Gray
Write-Host "    proving that the binary has not been modified or corrupted after build." -ForegroundColor Gray
Write-Host " 4. Never instruct end-users or customers to disable Windows Defender or SmartScreen." -ForegroundColor Gray
Write-Host " 5. To remove this certificate in the future, run:" -ForegroundColor Gray
Write-Host "    Get-ChildItem Cert:\CurrentUser\My | Where-Object Thumbprint -eq '$($newCert.Thumbprint)' | Remove-Item" -ForegroundColor DarkYellow
Write-Host "----------------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""
Write-Host "To use this certificate with build scripts, set:" -ForegroundColor Cyan
Write-Host "  `$env:SIGN_CERT_SHA1 = `"$($newCert.Thumbprint)`"" -ForegroundColor White
Write-Host ""

return $newCert.Thumbprint
