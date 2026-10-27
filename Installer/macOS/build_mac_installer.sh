#!/bin/bash
# ==============================================================
# Producer Toy ProVerb Official macOS Builder & Packager
# Generates Universal Binaries (Apple Silicon arm64 + Intel x86_64)
# Outputs:
#   1. Output/ProVerb.pkg (Apple Installer Package)
#   2. Output/ProVerb.dmg (Branded Apple Disk Image)
# ==============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
OUTPUT_DIR="${ROOT_DIR}/Output"
STAGING_DIR="${SCRIPT_DIR}/staging"
DMG_STAGING="${SCRIPT_DIR}/dmg_staging"

echo "=========================================================="
echo " Building Producer Toy ProVerb for macOS (Universal Binary)"
echo "=========================================================="

mkdir -p "${OUTPUT_DIR}"
rm -rf "${STAGING_DIR}" "${DMG_STAGING}"
mkdir -p "${STAGING_DIR}/VST3/Library/Audio/Plug-Ins/VST3"
mkdir -p "${STAGING_DIR}/AU/Library/Audio/Plug-Ins/Components"
mkdir -p "${STAGING_DIR}/AAX/Library/Application Support/Avid/Audio/Plug-Ins"

# 1. Compile with CMake for both arm64 (Apple Silicon) and x86_64 (Intel)
cd "${ROOT_DIR}"
echo "[1/4] Configuring & Building Universal Binaries..."
cmake -B build_mac \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13"

cmake --build build_mac --config Release --parallel

# 2. Stage Built Artifacts
echo "[2/4] Staging VST3 and AU plugins..."

VST3_PATH=""
if [ -d "build_mac/ProVerb_artefacts/Release/VST3/ProVerb.vst3" ]; then
    VST3_PATH="build_mac/ProVerb_artefacts/Release/VST3/ProVerb.vst3"
elif [ -d "build_mac/ProVerb_artefacts/VST3/ProVerb.vst3" ]; then
    VST3_PATH="build_mac/ProVerb_artefacts/VST3/ProVerb.vst3"
fi

AU_PATH=""
if [ -d "build_mac/ProVerb_artefacts/Release/AU/ProVerb.component" ]; then
    AU_PATH="build_mac/ProVerb_artefacts/Release/AU/ProVerb.component"
elif [ -d "build_mac/ProVerb_artefacts/AU/ProVerb.component" ]; then
    AU_PATH="build_mac/ProVerb_artefacts/AU/ProVerb.component"
fi

AAX_PATH=""
if [ -d "build_mac/ProVerb_artefacts/Release/AAX/ProVerb.aaxplugin" ]; then
    AAX_PATH="build_mac/ProVerb_artefacts/Release/AAX/ProVerb.aaxplugin"
elif [ -d "build_mac/ProVerb_artefacts/AAX/ProVerb.aaxplugin" ]; then
    AAX_PATH="build_mac/ProVerb_artefacts/AAX/ProVerb.aaxplugin"
fi

if [ -n "${VST3_PATH}" ]; then
    echo "  -> Staging VST3 from: ${VST3_PATH}"
    cp -R "${VST3_PATH}" "${STAGING_DIR}/VST3/Library/Audio/Plug-Ins/VST3/"
else
    echo "[ERROR] ProVerb.vst3 not found in build output!"
    exit 1
fi

if [ -n "${AU_PATH}" ]; then
    echo "  -> Staging AU from: ${AU_PATH}"
    cp -R "${AU_PATH}" "${STAGING_DIR}/AU/Library/Audio/Plug-Ins/Components/"
else
    echo "[ERROR] ProVerb.component (AU) not found in build output!"
    exit 1
fi

if [ -n "${AAX_PATH}" ]; then
    echo "  -> Staging AAX from: ${AAX_PATH}"
    cp -R "${AAX_PATH}" "${STAGING_DIR}/AAX/Library/Application Support/Avid/Audio/Plug-Ins/"
else
    echo "  -> AAX bundle not built; creating component placeholder..."
    touch "${STAGING_DIR}/AAX/Library/Application Support/Avid/Audio/Plug-Ins/.placeholder"
fi

# 3. Build Component Packages using pkgbuild
echo "[3/4] Building Component Packages (pkgbuild)..."
pkgbuild --root "${STAGING_DIR}/VST3" \
         --identifier "com.producertoy.proverb.vst3" \
         --version "1.0.0" \
         --install-location "/" \
         "${SCRIPT_DIR}/ProVerb_VST3.pkg"

pkgbuild --root "${STAGING_DIR}/AU" \
         --identifier "com.producertoy.proverb.au" \
         --version "1.0.0" \
         --install-location "/" \
         "${SCRIPT_DIR}/ProVerb_AU.pkg"

pkgbuild --root "${STAGING_DIR}/AAX" \
         --identifier "com.producertoy.proverb.aax" \
         --version "1.0.0" \
         --install-location "/" \
         "${SCRIPT_DIR}/ProVerb_AAX.pkg"

# 4. Assemble Final Package using productbuild
FINAL_PKG="${OUTPUT_DIR}/ProVerb.pkg"
echo "[4/4] Assembling Multi-Format Installer (productbuild)..."

productbuild --distribution "${SCRIPT_DIR}/distribution.xml" \
             --resources "${SCRIPT_DIR}" \
             --package-path "${SCRIPT_DIR}" \
             "${FINAL_PKG}"

echo "[OK] Generated: ${FINAL_PKG}"

# 5. Build Branded macOS Disk Image (.dmg)
FINAL_DMG="${OUTPUT_DIR}/ProVerb.dmg"
echo "Creating Branded Disk Image (.dmg)..."

mkdir -p "${DMG_STAGING}"
cp "${FINAL_PKG}" "${DMG_STAGING}/Install ProVerb.pkg"
cp "${SCRIPT_DIR}/Uninstall ProVerb.command" "${DMG_STAGING}/Uninstall ProVerb.command"
chmod +x "${DMG_STAGING}/Uninstall ProVerb.command"
cp "${SCRIPT_DIR}/README.txt" "${DMG_STAGING}/README.txt"

# Copy custom volume icon if available
if [ -f "${SCRIPT_DIR}/installer_box.icns" ]; then
    cp "${SCRIPT_DIR}/installer_box.icns" "${DMG_STAGING}/.VolumeIcon.icns"
fi

# Clean previous dmg if exists
rm -f "${FINAL_DMG}"

# Create compressed DMG using hdiutil
hdiutil create -volname "ProVerb" \
               -srcfolder "${DMG_STAGING}" \
               -ov -format UDZO \
               "${FINAL_DMG}"

if [ -f "${SCRIPT_DIR}/installer_box.icns" ]; then
    # Set volume icon attribute via SetFile if available
    if command -v SetFile &> /dev/null; then
        SetFile -a C "${FINAL_DMG}" || true
    fi
fi

# Cleanup temporary files
rm -rf "${SCRIPT_DIR}/ProVerb_VST3.pkg" "${SCRIPT_DIR}/ProVerb_AU.pkg" "${SCRIPT_DIR}/ProVerb_AAX.pkg" "${STAGING_DIR}" "${DMG_STAGING}"

echo "=========================================================="
echo " ALL macOS PACKAGES BUILT SUCCESSFULLY!"
echo " 1. Installer Package : ${FINAL_PKG}"
echo " 2. Apple Disk Image  : ${FINAL_DMG}"
echo "=========================================================="
