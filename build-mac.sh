#!/bin/bash
# Bygger VoxPilot som Audio Unit (+VST3) og installerer i ~/Library/Audio/Plug-Ins
set -euo pipefail
cd "$(dirname "$0")"

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target VoxPilot_AU VoxPilot_VST3 -j "$(sysctl -n hw.ncpu)"

COMP="$HOME/Library/Audio/Plug-Ins/Components/VoxPilot.component"
VST3="$HOME/Library/Audio/Plug-Ins/VST3/VoxPilot.vst3"
# ad-hoc signering (kreves for arm64 / Logic)
codesign --force --deep -s - "$COMP"
[ -d "$VST3" ] && codesign --force --deep -s - "$VST3"

# få macOS til å oppdage den nye komponenten
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "--- Validerer Audio Unit (auval) ---"
auval -v aufx Vxpl Sbau | tail -n 5
echo "Ferdig: $COMP"
