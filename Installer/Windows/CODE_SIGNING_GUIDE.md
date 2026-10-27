# ProVerb — Code Signing & Integrity Verification Guide

This guide describes the **zero-cost self-signed Authenticode code-signing workflow** for **ProducerToy ProVerb** (Windows x64 / x86 32-bit VST3 plugin and Inno Setup installer).

---

## 1. Overview & Objectives

The purpose of this setup is to:
1. **Digitally sign local development builds and installers** using standard Windows SDK Authenticode tools (`signtool.exe`).
2. **Verify cryptographic integrity** to ensure binaries and installer packages have not suffered corruption or tampering.
3. **Establish a repeatable signing pipeline** ready for seamless drop-in transition to a commercial public CA (OV/EV) certificate without modifying build scripts or installer scripts.

> [!IMPORTANT]
> **Windows SmartScreen & Trust Notice**:
> A self-signed certificate is stored exclusively in the developer's local `Cert:\CurrentUser\My` store. It is **NOT** recognized by default on external customer machines and will **NOT** eliminate the Windows Defender SmartScreen *"Unknown Publisher"* warning.
> **Never** instruct end users or customers to disable Windows Defender, SmartScreen, or Antivirus protection.

---

## 2. Creating the Development Certificate (Phase 2)

A dedicated PowerShell utility is provided to generate an isolated, secure self-signed code-signing certificate:

```powershell
# Run from repository root:
powershell -ExecutionPolicy Bypass -File .\Installer\Windows\create_dev_certificate.ps1
```

### Certificate Specifications
- **Subject**: `CN=ProducerToy ProVerb Development`
- **Enhanced Key Usage (EKU)**: Code Signing (`1.3.6.1.5.5.7.3.3`)
- **Key Specification**: `Signature` (AT_SIGNATURE)
- **Key Algorithm / Length**: RSA 2048-bit
- **Hash Algorithm**: SHA-256
- **Storage Location**: `Cert:\CurrentUser\My` (Current User Personal Store)
- **Validity**: 2 Years (730 days)

### Idempotency and Safety
- **Idempotent**: The script automatically checks `Cert:\CurrentUser\My` for an existing valid certificate matching `CN=ProducerToy ProVerb Development`. If found, it prints the existing thumbprint and avoids generating duplicate certificates.
- **Store Safety**: The script does **NOT** install anything into `LocalMachine\Root` (Trusted Root Certification Authorities) or `LocalMachine\TrustedPublisher`.
- **Zero Key Leakage**: Private keys are kept securely inside the Windows CryptoAPI/CNG store; no `.pfx` or `.key` files are written to disk or committed to Git.

---

## 3. Signing Local Builds and Installers (Phase 3)

### Option A: Automatic Pipeline via `package_all_installers.bat`
When you build the installer, signing occurs automatically at each stage:
```cmd
package_all_installers.bat
```
1. Signs 64-bit VST3 binary (`build\...\x86_64-win\ProVerb.vst3`).
2. Compiles Inno Setup installer (`Output\ProducerToy_ProVerb_Windows_Installer.exe`).
3. Signs the final output installer and copies to root (`ProducerToy_ProVerb_Installer.exe`).

### Option B: PowerShell Code Signing Script
To sign default binaries or custom targets:
```powershell
# Sign all default build artifacts (VST3 x64, VST3 x86, Installers):
powershell -ExecutionPolicy Bypass -File .\Installer\Windows\sign_build.ps1

# Sign without RFC 3161 timestamping (for offline builds):
powershell -ExecutionPolicy Bypass -File .\Installer\Windows\sign_build.ps1 -NoTimestamp

# Sign specific target files:
powershell -ExecutionPolicy Bypass -File .\Installer\Windows\sign_build.ps1 -Artifacts ".\Output\ProducerToy_ProVerb_Windows_Installer.exe"

# Specify a specific certificate thumbprint:
powershell -ExecutionPolicy Bypass -File .\Installer\Windows\sign_build.ps1 -Thumbprint "14BFF694E3549D7DE1821A00B11189DEBA5E8628"
```

### Option C: Batch Utility `sign_binaries.bat`
Used directly in CI/CD or command line:
```cmd
Installer\Windows\sign_binaries.bat "path\to\binary.exe"
```

---

## 4. Verifying Signatures and File Integrity (Phase 4)

A dedicated verification tool inspects each artifact:
```powershell
# Verify all canonical artifacts:
powershell -ExecutionPolicy Bypass -File .\Installer\Windows\verify_signatures.ps1

# Verify a single file:
powershell -ExecutionPolicy Bypass -File .\Installer\Windows\verify_signatures.ps1 -Artifacts ".\ProducerToy_ProVerb_Installer.exe"
```

### What Verification Checks:
1. **File Existence & Physical Size**: Confirms the file exists and reports size in MB.
2. **Cryptographic SHA-256 Hash**: Computes and displays the SHA-256 digest.
3. **Authenticode Signature Integrity**: Verifies that the binary has not been tampered with or modified after signing (`signtool` / `Get-AuthenticodeSignature`).
4. **Trust Distinction**:
   - `VALID (Intact)` + `SELF-SIGNED (Untrusted Root / Dev)`: The file has an intact, untampered signature created by the local dev certificate.
   - `VALID (Intact)` + `TRUSTED (Public Commercial CA)`: The file was signed with a publicly trusted commercial certificate.
   - `TAMPERED / CORRUPT`: Digest mismatch — file was modified after signing!
   - `UNSIGNED`: No signature exists.

---

## 5. Removing the Development Certificate

To remove the self-signed development certificate from your Windows Certificate Store:

```powershell
# 1. View the development certificate:
Get-ChildItem -Path Cert:\CurrentUser\My | Where-Object { $_.Subject -like "*ProducerToy ProVerb Development*" }

# 2. Delete the development certificate:
Get-ChildItem -Path Cert:\CurrentUser\My | Where-Object { $_.Subject -like "*ProducerToy ProVerb Development*" } | Remove-Item

# 3. Verify removal:
Get-ChildItem -Path Cert:\CurrentUser\My | Where-Object { $_.Subject -like "*ProducerToy ProVerb Development*" }
```

---

## 6. Commercial Production Signing Transition

When preparing for official public commercial distribution:

1. **Obtain Commercial Certificate**: Purchase a standard OV (Organization Validation) or EV (Extended Validation) code-signing certificate from an authorized public CA (e.g., DigiCert, Sectigo, GlobalSign).
2. **Environment Variables**: Provide credentials via secure CI/CD secrets:
   - For Hardware Security Module (HSM) / Certificate Store:
     ```cmd
     set SIGN_CERT_SHA1=<COMMERCIAL_CERT_THUMBPRINT>
     ```
   - For file-based PFX (if using standard OV):
     ```cmd
     set SIGN_CERT_FILE=C:\secure\path\release_cert.pfx
     set SIGN_CERT_PASSWORD=YourSecurePassword
     ```
3. **Run Pipeline**: Run `package_all_installers.bat` without changing any code. The signing scripts will automatically utilize the commercial credentials, yielding `TRUSTED (Public Commercial CA)` verification.
