#!/bin/bash
# Build Wave Emulation (resizable-window version) on macOS.
# Requires: Xcode Command Line Tools (xcode-select --install) and Homebrew cmake + ninja
#   brew install cmake ninja
# Usage:  ./build-mac.sh            -> build only
#         ./build-mac.sh --install  -> build and install into your user folders
set -euo pipefail
cd "$(dirname "$0")"

# Build only for this Mac's architecture (faster than the universal release build).
ARCH="$(uname -m)"
cmake -S . -B build-mac -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$ARCH"
cmake --build build-mac --target WaveEmulation_Standalone WaveEmulation_AU WaveEmulation_VST3

OUT="build-mac/WaveEmulation_artefacts/Release"
echo
echo "Built:"
ls -d "$OUT/Standalone/"*.app "$OUT/AU/"*.component "$OUT/VST3/"*.vst3

if [[ "${1:-}" == "--install" ]]; then
    mkdir -p "$HOME/Applications" "$HOME/Library/Audio/Plug-Ins/Components" "$HOME/Library/Audio/Plug-Ins/VST3"
    rm -rf "$HOME/Applications/Wave Emulation.app" \
           "$HOME/Library/Audio/Plug-Ins/Components/Wave Emulation.component" \
           "$HOME/Library/Audio/Plug-Ins/VST3/Wave Emulation.vst3"
    cp -R "$OUT/Standalone/Wave Emulation.app" "$HOME/Applications/"
    cp -R "$OUT/AU/Wave Emulation.component" "$HOME/Library/Audio/Plug-Ins/Components/"
    cp -R "$OUT/VST3/Wave Emulation.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/"
    # Local builds are not notarised; remove the quarantine flag just in case.
    xattr -dr com.apple.quarantine "$HOME/Applications/Wave Emulation.app" 2>/dev/null || true
    killall -9 AudioComponentRegistrar 2>/dev/null || true
    echo
    echo "Installed to ~/Applications and ~/Library/Audio/Plug-Ins."
    echo "If the original 0.1.6 plug-ins are still in /Library/Audio/Plug-Ins, remove them"
    echo "so your DAW does not see two plug-ins with the same ID."
fi
