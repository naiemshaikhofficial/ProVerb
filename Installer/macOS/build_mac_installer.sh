#!/bin/bash
# ==============================================================
# ProducerToy ProVerb Official macOS Universal (.pkg) Builder
# Builds VST3 + AU (Audio Unit) Universal Binaries (Apple Silicon + Intel)
# ==============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
OUTPUT_DIR="${ROOT_DIR}/Output"
STAGING_DIR="${SCRIPT_DIR}/staging"

echo "=========================================================="
echo " Building ProducerToy ProVerb for macOS (Universal Binary)"
echo "=========================================================="

mkdir -p "${OUTPUT_DIR}"
rm -rf "${STAGING_DIR}"
mkdir -p "${STAGING_DIR}/VST3/Library/Audio/Plug-Ins/VST3"
mkdir -p "${STAGING_DIR}/AU/Library/Audio/Plug-Ins/Components"
mkdir -p "${STAGING_DIR}/AAX/Library/Application Support/Avid/Audio/Plug-Ins"

# 1. Compile with CMake for both arm64 and x86_64
cd "${ROOT_DIR}"
cmake -B build_mac \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13"

cmake --build build_mac --config Release --parallel

# 2. Stage Built Artifacts
echo "Locating built VST3 and AU artifacts..."

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
    echo "Staging VST3 from: ${VST3_PATH}"
    cp -R "${VST3_PATH}" "${STAGING_DIR}/VST3/Library/Audio/Plug-Ins/VST3/"
else
    echo "ERROR: ProVerb.vst3 not found in build output!"
    exit 1
fi

if [ -n "${AU_PATH}" ]; then
    echo "Staging AU from: ${AU_PATH}"
    cp -R "${AU_PATH}" "${STAGING_DIR}/AU/Library/Audio/Plug-Ins/Components/"
else
    echo "ERROR: ProVerb.component (AU) not found in build output!"
    exit 1
fi

if [ -n "${AAX_PATH}" ]; then
    echo "Staging AAX from: ${AAX_PATH}"
    cp -R "${AAX_PATH}" "${STAGING_DIR}/AAX/Library/Application Support/Avid/Audio/Plug-Ins/"
else
    echo "[NOTICE] AAX bundle not present; creating placeholder for optional component..."
    touch "${STAGING_DIR}/AAX/Library/Application Support/Avid/Audio/Plug-Ins/.placeholder"
fi

# 3. Build Component Packages using pkgbuild
echo "Building VST3 Component Package..."
pkgbuild --root "${STAGING_DIR}/VST3" \
         --identifier "com.producertoy.proverb.vst3" \
         --version "1.0.0" \
         --install-location "/" \
         "${SCRIPT_DIR}/ProVerb_VST3.pkg"

echo "Building AU Component Package..."
pkgbuild --root "${STAGING_DIR}/AU" \
         --identifier "com.producertoy.proverb.au" \
         --version "1.0.0" \
         --install-location "/" \
         "${SCRIPT_DIR}/ProVerb_AU.pkg"

echo "Building AAX Component Package..."
pkgbuild --root "${STAGING_DIR}/AAX" \
         --identifier "com.producertoy.proverb.aax" \
         --version "1.0.0" \
         --install-location "/" \
         "${SCRIPT_DIR}/ProVerb_AAX.pkg"

# 4. Assemble Final Multi-Format Installer using productbuild
FINAL_PKG="${OUTPUT_DIR}/ProducerToy_ProVerb_macOS_Installer.pkg"
echo "Assembling final installer into ${FINAL_PKG}..."

productbuild --distribution "${SCRIPT_DIR}/distribution.xml" \
             --resources "${SCRIPT_DIR}" \
             --package-path "${SCRIPT_DIR}" \
             "${FINAL_PKG}"

# Cleanup temporary intermediate component packages & staging
rm -rf "${SCRIPT_DIR}/ProVerb_VST3.pkg" "${SCRIPT_DIR}/ProVerb_AU.pkg" "${SCRIPT_DIR}/ProVerb_AAX.pkg" "${STAGING_DIR}"

echo "=========================================================="
echo " SUCCESS! macOS Installer generated:"
echo " ${FINAL_PKG}"
echo "=========================================================="
