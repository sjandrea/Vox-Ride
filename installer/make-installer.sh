#!/bin/bash
# Lager VoxPilot-<versjon>.pkg – en vanlig macOS-installer (dobbeltklikk → Installer).
#
#   installer/make-installer.sh <VoxPilot.component> <VoxPilot.vst3> <versjon> [utmappe]
#
# Pluginene installeres for alle brukere i /Library/Audio/Plug-Ins/{Components,VST3}.
# Valgfritt: sett INSTALLER_SIGN_IDENTITY="Developer ID Installer: Navn (TEAMID)" for å signere pakken.
set -euo pipefail

COMPONENT="$1"; VST3="$2"; VERSION="$3"; OUT="${4:-.}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/au/Library/Audio/Plug-Ins/Components" "$WORK/vst3/Library/Audio/Plug-Ins/VST3" "$WORK/pkgs" "$OUT"
ditto "$COMPONENT" "$WORK/au/Library/Audio/Plug-Ins/Components/VoxPilot.component"
ditto "$VST3"      "$WORK/vst3/Library/Audio/Plug-Ins/VST3/VoxPilot.vst3"

# Hindre at Installer «flytter» installasjonen til en eldre kopi den finner et annet sted på disken
make_component_plist () {
    pkgbuild --analyze --root "$1" "$2" >/dev/null
    /usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" "$2"
}
make_component_plist "$WORK/au"   "$WORK/au.plist"
make_component_plist "$WORK/vst3" "$WORK/vst3.plist"

pkgbuild --root "$WORK/au" --component-plist "$WORK/au.plist" \
         --identifier com.sbaudio.voxpilot.au --version "$VERSION" \
         --install-location / --scripts "$HERE/scripts" "$WORK/pkgs/VoxPilot-AU.pkg"
pkgbuild --root "$WORK/vst3" --component-plist "$WORK/vst3.plist" \
         --identifier com.sbaudio.voxpilot.vst3 --version "$VERSION" \
         --install-location / "$WORK/pkgs/VoxPilot-VST3.pkg"

sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
mkdir -p "$WORK/resources"
for f in "$HERE"/resources/*.html; do sed "s/@VERSION@/$VERSION/g" "$f" > "$WORK/resources/$(basename "$f")"; done

PKG="$OUT/VoxPilot-$VERSION.pkg"
SIGN_ARGS=()
if [ -n "${INSTALLER_SIGN_IDENTITY:-}" ]; then SIGN_ARGS=(--sign "$INSTALLER_SIGN_IDENTITY"); fi

productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/resources" \
             --package-path "$WORK/pkgs" ${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"} "$PKG"

echo "Laget $PKG"
