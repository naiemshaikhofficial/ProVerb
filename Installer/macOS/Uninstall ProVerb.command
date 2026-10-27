#!/bin/bash
# ==============================================================
# Producer Toy ProVerb Official macOS Uninstaller
# ==============================================================

clear
echo "=========================================================="
echo "          Producer Toy ProVerb macOS Uninstaller"
echo "=========================================================="
echo ""
echo "This utility will remove ProVerb plugin components from your Mac:"
echo " • VST3 Plugin    : /Library/Audio/Plug-Ins/VST3/ProVerb.vst3"
echo " • Audio Unit (AU): /Library/Audio/Plug-Ins/Components/ProVerb.component"
echo " • Avid AAX Plugin: /Library/Application Support/Avid/Audio/Plug-Ins/ProVerb.aaxplugin"
echo " • Presets & Data : /Library/Audio/Presets/ProducerToy/ProVerb"
echo ""

read -p "Are you sure you want to completely uninstall ProVerb? (y/N): " CONFIRM
if [[ "$CONFIRM" =~ ^[Yy]$ ]]; then
    echo ""
    echo "Requesting administrator credentials to remove system plugin files..."
    sudo rm -rf "/Library/Audio/Plug-Ins/VST3/ProVerb.vst3"
    sudo rm -rf "/Library/Audio/Plug-Ins/Components/ProVerb.component"
    sudo rm -rf "/Library/Application Support/Avid/Audio/Plug-Ins/ProVerb.aaxplugin"
    sudo rm -rf "/Library/Audio/Presets/ProducerToy/ProVerb"
    sudo rm -rf "/Library/Application Support/ProducerToy/ProVerb"

    # Remove pkg receipts from macOS database
    sudo pkgutil --forget com.producertoy.proverb.vst3 2>/dev/null || true
    sudo pkgutil --forget com.producertoy.proverb.au 2>/dev/null || true
    sudo pkgutil --forget com.producertoy.proverb.aax 2>/dev/null || true

    echo ""
    echo "=========================================================="
    echo " [SUCCESS] ProVerb has been completely removed from your Mac."
    echo "=========================================================="
else
    echo ""
    echo "Uninstallation cancelled. No files were removed."
fi

echo ""
read -n 1 -s -r -p "Press any key to close this window..."
echo ""
