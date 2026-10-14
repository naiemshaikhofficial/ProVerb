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
echo "Staging VST3, AU, and AAX plugins..."
cp -R "build_mac/ProVerb_artefacts/Release/VST3/ProVerb.vst3" \
      "${STAGING_DIR}/VST3/Library/Audio/Plug-Ins/VST3/"

cp -R "build_mac/ProVerb_artefacts/Release/AU/ProVerb.component" \
      "${STAGING_DIR}/AU/Library/Audio/Plug-Ins/Components/"

if [ -d "build_mac/ProVerb_artefacts/Release/AAX/ProVerb.aaxplugin" ]; then
    cp -R "build_mac/ProVerb_artefacts/Release/AAX/ProVerb.aaxplugin" \
          "${STAGING_DIR}/AAX/Library/Application Support/Avid/Audio/Plug-Ins/"
else
    echo "[NOTICE] AAX bundle not present; creating stub package for distribution.xml..."
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
