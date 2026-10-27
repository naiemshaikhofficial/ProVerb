# ProducerToy ProVerb - Official macOS Release Guide

This guide details the official **Universal Binary (Apple Silicon + Intel)** release pipeline for macOS containing **Audio Unit (AU)** and **VST3** formats.

---

## Supported Formats on macOS

1. **Audio Unit (AU / `.component`):**
   - Path: `/Library/Audio/Plug-Ins/Components/ProVerb.component`
   - Primary for: **Apple Logic Pro**, **GarageBand**, **Ableton Live (Mac)**
2. **VST3 (`.vst3`):**
   - Path: `/Library/Audio/Plug-Ins/VST3/ProVerb.vst3`
   - Primary for: **FL Studio (Mac)**, **Ableton Live**, **Cubase**, **Studio One**, **Reaper**, **Bitwig**
3. **AAX (`.aaxplugin`):**
   - Path: `/Library/Application Support/Avid/Audio/Plug-Ins/ProVerb.aaxplugin`
   - Primary for: **Avid Pro Tools 11+**

---

## Generated Release Outputs

When built, two production artifacts are output to `Output/`:
- **`Output/ProVerb.pkg`**: Standalone Apple Installer package with interactive graphical installer and component choices.
- **`Output/ProVerb.dmg`**: Sleek branded Apple Disk Image containing:
  - `Install ProVerb.pkg` (1-click installer)
  - `Uninstall ProVerb.command` (1-click uninstaller)
  - `README.txt` (Quick Start Guide)
  - `.VolumeIcon.icns` (Branded 3D Box disk volume icon)

---

## 1-Command Local Build (on macOS Machine)

In Terminal:
```bash
chmod +x Installer/macOS/build_mac_installer.sh
./Installer/macOS/build_mac_installer.sh
```

---

## Automated Cloud Build (GitHub Actions)

Any push or pull request to `main` automatically runs `.github/workflows/build_mac_windows_installers.yml` on an Apple runner (`macos-latest`).
It builds:
- Universal `arm64` + `x86_64` binaries
- `ProVerb.pkg`
- `ProVerb.dmg`
- Uploads both as downloadable GitHub workflow release artifacts.
