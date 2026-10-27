<#
.SYNOPSIS
    Verifies Authenticode signatures and SHA-256 integrity hashes of ProVerb build artifacts.

.DESCRIPTION
    Audits PE binaries (VST3 64-bit, VST3 32-bit) and Inno Setup installers to:
    1. Verify file existence and physical file size.
    2. Compute SHA-256 cryptographic file hashes.
    3. Verify embedded Authenticode digital signatures.
    4. Distinguish between cryptographic signature integrity (unaltered binary)
       and public CA trust status (self-signed vs commercial root).
    5. Report any unsigned, tampered, or missing artifacts with non-zero exit code.

.PARAMETER Artifacts
    Specific file paths to verify. Defaults to canonical VST3 PE binaries and installers.

.EXAMPLE
    .\verify_signatures.ps1
#>

[CmdletBinding()]
param(
    [string[]]$Artifacts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "        ProducerToy ProVerb - Authenticode Signature Verification" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Resolve-Path (Join-Path $scriptDir "..\..")

if (-not $Artifacts -or $Artifacts.Count -eq 0) {
    $Artifacts = @(
        (Join-Path $projectRoot "build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\Contents\x86_64-win\ProVerb.vst3"),
        (Join-Path $projectRoot "build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\Contents\x86-win\ProVerb.vst3"),
        (Join-Path $projectRoot "Output\ProducerToy_ProVerb_Windows_Installer.exe"),
        (Join-Path $projectRoot "ProducerToy_ProVerb_Installer.exe")
    )
}

$results = @()
$hasFailure = $false

foreach ($target in $Artifacts) {
    $exists = Test-Path $target
    $resolvedPath = if ($exists) { (Resolve-Path $target).Path } else { $target }
    $fileName = Split-Path -Leaf $resolvedPath

    if (-not $exists) {
        $results += [PSCustomObject]@{
            File        = $fileName
            Exists      = $false
            SizeMB      = "N/A"
            SHA256      = "MISSING"
            Signed      = $false
            Integrity   = "FILE_MISSING"
            TrustStatus = "N/A"
            Subject     = "N/A"
            Thumbprint  = "N/A"
            Expires     = "N/A"
        }
        $hasFailure = $true
        continue
    }

    $fileInfo = Get-Item -Path $resolvedPath
    $sizeMb = [Math]::Round($fileInfo.Length / 1MB, 2)
    $hash = (Get-FileHash -Path $resolvedPath -Algorithm SHA256).Hash
    $sig = Get-AuthenticodeSignature -FilePath $resolvedPath

    $isSigned = ($sig.Status -ne 'NotSigned')
    $integrity = "UNKNOWN"
    $trustStatus = "UNKNOWN"
    $subject = "N/A"
    $thumbprint = "N/A"
    $expires = "N/A"

    if ($sig.Status -eq 'NotSigned') {
        $integrity = "UNSIGNED"
        $trustStatus = "No signature found"
        $hasFailure = $true
    }
    elseif ($sig.Status -eq 'HashMismatch') {
        $integrity = "TAMPERED / CORRUPT"
        $trustStatus = "Digest mismatch"
        $hasFailure = $true
    }
    elseif ($sig.Status -eq 'Valid') {
        $integrity = "VALID (Intact)"
        $trustStatus = "TRUSTED (Public Commercial CA)"
        if ($sig.SignerCertificate) {
            $subject = $sig.SignerCertificate.Subject
            $thumbprint = $sig.SignerCertificate.Thumbprint
            $expires = $sig.SignerCertificate.NotAfter.ToString("yyyy-MM-dd")
        }
    }
    elseif ($sig.Status -eq 'UnknownError' -or $sig.Status -eq 'NotTrusted') {
        # Untrusted root: Valid signature structure and hash, but self-signed or not in Windows root store
        $integrity = "VALID (Intact)"
        $trustStatus = "SELF-SIGNED (Untrusted Root / Dev)"
        if ($sig.SignerCertificate) {
            $subject = $sig.SignerCertificate.Subject
            $thumbprint = $sig.SignerCertificate.Thumbprint
            $expires = $sig.SignerCertificate.NotAfter.ToString("yyyy-MM-dd")
        }
    }
    else {
        $integrity = "INVALID ($($sig.Status))"
        $trustStatus = $sig.StatusMessage
        $hasFailure = $true
    }

    $results += [PSCustomObject]@{
        File        = $fileName
        Exists      = $true
        SizeMB      = "$sizeMb MB"
        SHA256      = $hash.Substring(0, 16) + "..."
        Signed      = $isSigned
        Integrity   = $integrity
        TrustStatus = $trustStatus
        Subject     = $subject
        Thumbprint  = if ($thumbprint.Length -gt 16) { $thumbprint.Substring(0, 16) + "..." } else { $thumbprint }
        Expires     = $expires
        FullPath    = $resolvedPath
        FullSHA256  = $hash
    }
}

# Display Formatted Table
Write-Host "AUDIT REPORT: ARTIFACT SIGNATURE & INTEGRITY TABLE" -ForegroundColor Yellow
Write-Host "---------------------------------------------------------------------------------------------------------------" -ForegroundColor DarkGray

foreach ($r in $results) {
    $statusColor = if ($r.Integrity -like "VALID*") { 'Green' } else { 'Red' }
    Write-Host "FILE: $($r.File)" -ForegroundColor Cyan
    Write-Host "  Path        : $($r.FullPath)" -ForegroundColor DarkGray
    Write-Host "  Size        : $($r.SizeMB)" -ForegroundColor Gray
    Write-Host "  SHA-256     : $($r.FullSHA256)" -ForegroundColor White
    Write-Host "  Integrity   : $($r.Integrity)" -ForegroundColor $statusColor
    Write-Host "  Trust Status: $($r.TrustStatus)" -ForegroundColor $(if ($r.TrustStatus -like "*TRUSTED*") { 'Green' } else { 'Yellow' })
    if ($r.Subject -ne "N/A") {
        Write-Host "  Signer      : $($r.Subject)" -ForegroundColor Gray
        Write-Host "  Thumbprint  : $($r.Thumbprint)" -ForegroundColor Gray
        Write-Host "  Expires     : $($r.Expires)" -ForegroundColor Gray
    }
    Write-Host "---------------------------------------------------------------------------------------------------------------" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "                       VERIFICATION SUMMARY" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan

$validCount = @($results | Where-Object { $_.Integrity -like "VALID*" }).Count
$totalCount = @($results).Count

Write-Host "  Total Artifacts Checked  : $totalCount" -ForegroundColor White
Write-Host "  Cryptographically Valid  : $validCount" -ForegroundColor $(if ($validCount -eq $totalCount) { 'Green' } else { 'Yellow' })
Write-Host "  Failed / Unsigned / Bad  : $($totalCount - $validCount)" -ForegroundColor $(if ($hasFailure) { 'Red' } else { 'Green' })
Write-Host ""

if ($hasFailure) {
    Write-Host "[FAIL] One or more artifacts failed integrity verification or are missing." -ForegroundColor Red
    exit 1
}

Write-Host "[PASS] All checked artifacts possess valid, untampered Authenticode signatures!" -ForegroundColor Green
exit 0
