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

# Check for --skip-build flag
SKIP_BUILD=false
for arg in "$@"; do
    if [ "$arg" == "--skip-build" ]; then
        SKIP_BUILD=true
    fi
done

cd "${ROOT_DIR}"
if [ "$SKIP_BUILD" = false ]; then
    echo "[1/5] Configuring CMake for Universal Binaries (arm64 + x86_64)..."
    cmake -B build_mac \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
          -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15"

    echo "[2/5] Compiling ProVerb VST3 and Audio Unit (AU) targets..."
    cmake --build build_mac --config Release --target ProVerb_VST3 --parallel
    cmake --build build_mac --config Release --target ProVerb_AU --parallel
else
    echo "[INFO] Skipping CMake build step (--skip-build flag specified)."
fi

# 2. Locate and Stage Built Artifacts
echo "[3/5] Locating and staging plugin bundles..."

VST3_PATH=""
if [ -d "build_mac/ProVerb_artefacts/Release/VST3/ProVerb.vst3" ]; then
    VST3_PATH="build_mac/ProVerb_artefacts/Release/VST3/ProVerb.vst3"
elif [ -d "build_mac/ProVerb_artefacts/VST3/ProVerb.vst3" ]; then
    VST3_PATH="build_mac/ProVerb_artefacts/VST3/ProVerb.vst3"
else
    VST3_PATH="$(find build_mac -name "ProVerb.vst3" -type d | head -n 1)"
fi

AU_PATH=""
if [ -d "build_mac/ProVerb_artefacts/Release/AU/ProVerb.component" ]; then
    AU_PATH="build_mac/ProVerb_artefacts/Release/AU/ProVerb.component"
elif [ -d "build_mac/ProVerb_artefacts/AU/ProVerb.component" ]; then
    AU_PATH="build_mac/ProVerb_artefacts/AU/ProVerb.component"
else
    AU_PATH="$(find build_mac -name "ProVerb.component" -type d | head -n 1)"
fi

AAX_PATH=""
if [ -d "build_mac/ProVerb_artefacts/Release/AAX/ProVerb.aaxplugin" ]; then
    AAX_PATH="build_mac/ProVerb_artefacts/Release/AAX/ProVerb.aaxplugin"
elif [ -d "build_mac/ProVerb_artefacts/AAX/ProVerb.aaxplugin" ]; then
    AAX_PATH="build_mac/ProVerb_artefacts/AAX/ProVerb.aaxplugin"
else
    AAX_PATH="$(find build_mac -name "ProVerb.aaxplugin" -type d | head -n 1)"
fi

if [ -n "${VST3_PATH}" ] && [ -d "${VST3_PATH}" ]; then
    echo "  -> Found VST3 at: ${VST3_PATH}"
    cp -R "${VST3_PATH}" "${STAGING_DIR}/VST3/Library/Audio/Plug-Ins/VST3/"
else
    echo "[ERROR] ProVerb.vst3 not found in build directory!"
    exit 1
fi

if [ -n "${AU_PATH}" ] && [ -d "${AU_PATH}" ]; then
    echo "  -> Found AU at: ${AU_PATH}"
    cp -R "${AU_PATH}" "${STAGING_DIR}/AU/Library/Audio/Plug-Ins/Components/"
else
    echo "[ERROR] ProVerb.component (AU) not found in build directory!"
    exit 1
fi

if [ -n "${AAX_PATH}" ] && [ -d "${AAX_PATH}" ]; then
    echo "  -> Found AAX at: ${AAX_PATH}"
    cp -R "${AAX_PATH}" "${STAGING_DIR}/AAX/Library/Application Support/Avid/Audio/Plug-Ins/"
else
    echo "  -> AAX format SDK not detected; creating placeholder..."
    touch "${STAGING_DIR}/AAX/Library/Application Support/Avid/Audio/Plug-Ins/.placeholder"
fi

# Bundle AI model inside Resources if available
if [ -f "${ROOT_DIR}/Assets/ProVerb_AI.onnx" ]; then
    echo "  -> Bundling ProVerb_AI.onnx into plugin Resources..."
    mkdir -p "${STAGING_DIR}/VST3/Library/Audio/Plug-Ins/VST3/ProVerb.vst3/Contents/Resources"
    mkdir -p "${STAGING_DIR}/AU/Library/Audio/Plug-Ins/Components/ProVerb.component/Contents/Resources"
    cp -f "${ROOT_DIR}/Assets/ProVerb_AI.onnx" "${STAGING_DIR}/VST3/Library/Audio/Plug-Ins/VST3/ProVerb.vst3/Contents/Resources/"
    cp -f "${ROOT_DIR}/Assets/ProVerb_AI.onnx" "${STAGING_DIR}/AU/Library/Audio/Plug-Ins/Components/ProVerb.component/Contents/Resources/"
fi

# Ad-hoc codesign for Gatekeeper and DAW validation
echo "  -> Applying ad-hoc code signature for Gatekeeper..."
if command -v codesign &> /dev/null; then
    codesign --force --deep --sign - "${STAGING_DIR}/VST3/Library/Audio/Plug-Ins/VST3/ProVerb.vst3" || true
    codesign --force --deep --sign - "${STAGING_DIR}/AU/Library/Audio/Plug-Ins/Components/ProVerb.component" || true
fi

# 3. Build Component Packages using pkgbuild
echo "[4/5] Building Component Packages (pkgbuild)..."
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
echo "  -> Assembling Multi-Format Installer (productbuild)..."

productbuild --distribution "${SCRIPT_DIR}/distribution.xml" \
             --resources "${SCRIPT_DIR}" \
             --package-path "${SCRIPT_DIR}" \
             "${FINAL_PKG}"

echo "[OK] Generated: ${FINAL_PKG}"

# 5. Build Branded macOS Disk Image (.dmg)
FINAL_DMG="${OUTPUT_DIR}/ProVerb.dmg"
echo "[5/5] Creating Branded Apple Disk Image (.dmg)..."

mkdir -p "${DMG_STAGING}"
cp "${FINAL_PKG}" "${DMG_STAGING}/Install ProVerb.pkg"
cp "${SCRIPT_DIR}/Uninstall ProVerb.command" "${DMG_STAGING}/Uninstall ProVerb.command"
chmod +x "${DMG_STAGING}/Uninstall ProVerb.command"
cp "${SCRIPT_DIR}/README.txt" "${DMG_STAGING}/README.txt"

# Copy custom volume icon if available
if [ -f "${SCRIPT_DIR}/installer_box.icns" ]; then
    cp "${SCRIPT_DIR}/installer_box.icns" "${DMG_STAGING}/.VolumeIcon.icns"
    if command -v SetFile &> /dev/null; then
        SetFile -a C "${DMG_STAGING}" || true
    fi
fi

# Clean previous dmg if exists
rm -f "${FINAL_DMG}"

# Create compressed DMG using hdiutil
hdiutil create -volname "ProVerb" \
               -srcfolder "${DMG_STAGING}" \
               -ov -format UDZO \
               "${FINAL_DMG}"

if [ -f "${SCRIPT_DIR}/installer_box.icns" ] && command -v SetFile &> /dev/null; then
    SetFile -a C "${FINAL_DMG}" || true
fi

# Calculate SHA256 checksums
if command -v shasum &> /dev/null; then
    cd "${OUTPUT_DIR}"
    shasum -a 256 ProVerb.dmg ProVerb.pkg > ProVerb_macOS_SHA256.txt
    echo "  -> Generated SHA-256 Checksums:"
    cat ProVerb_macOS_SHA256.txt
    cd "${ROOT_DIR}"
fi

# Cleanup temporary staging directories
rm -rf "${SCRIPT_DIR}/ProVerb_VST3.pkg" "${SCRIPT_DIR}/ProVerb_AU.pkg" "${SCRIPT_DIR}/ProVerb_AAX.pkg" "${STAGING_DIR}" "${DMG_STAGING}"

echo "=========================================================="
echo " ALL macOS PACKAGES BUILT SUCCESSFULLY!"
echo " 1. Installer Package : ${FINAL_PKG}"
echo " 2. Apple Disk Image  : ${FINAL_DMG}"
echo "=========================================================="
