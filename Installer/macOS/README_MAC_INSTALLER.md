# ProducerToy ProVerb - macOS Installer Guide

This guide explains how to generate the official **Universal Binary (Apple Silicon + Intel)** installer for macOS containing **VST3** and **Audio Unit (AU)** formats.

---

## What Formats Are Supported on macOS?

1. **Audio Unit (AU / `.component`):**
   - Location: `/Library/Audio/Plug-Ins/Components/ProVerb.component`
   - Required by: **Apple Logic Pro**, **GarageBand**, **Ableton Live (Mac)**
2. **VST3 (`.vst3`):**
   - Location: `/Library/Audio/Plug-Ins/VST3/ProVerb.vst3`
   - Required by: **FL Studio (Mac)**, **Ableton Live**, **Cubase**, **Studio One**, **Reaper**, **Bitwig**

---

## Prerequisites (macOS Machine or GitHub Actions Runner)

- macOS 10.15 (Catalina) or newer (macOS 12/13/14 recommended)
- Xcode 14+ and Command Line Tools (`xcode-select --install`)
- CMake 3.22+ (`brew install cmake`)

---

## 1-Command Build & Package

Open Terminal in this directory and execute:

```bash
chmod +x build_mac_installer.sh
./build_mac_installer.sh
```

### Output:
The completed installer will be placed in:
`Output/ProducerToy_ProVerb_macOS_Installer.pkg`

---

## Optional: Apple Developer Code Signing & Notarization

To sign with your Apple Developer ID:

```bash
productsign --sign "Developer ID Installer: YOUR_NAME (TEAM_ID)" \
            Output/ProducerToy_ProVerb_macOS_Installer.pkg \
            Output/ProducerToy_ProVerb_macOS_Installer_Signed.pkg

xcrun notarytool submit Output/ProducerToy_ProVerb_macOS_Installer_Signed.pkg \
      --keychain-profile "AC_PASSWORD" \
      --wait

xcrun stapler staple Output/ProducerToy_ProVerb_macOS_Installer_Signed.pkg
```
