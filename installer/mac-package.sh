#!/bin/bash
# Builds the macOS installer (.pkg) from the Release build: VST3 + AU for every plugin.
# Signing: with a Developer ID (secrets MACOS_CERT_P12 / MACOS_CERT_PASSWORD / MACOS_SIGN_ID, and
# MACOS_NOTARY_ID / MACOS_NOTARY_PASSWORD / MACOS_TEAM_ID for notarizing) it signs and notarizes;
# without them it signs "ad hoc" (works, but macOS asks you to allow it the first time).
set -e
VERSION="$1"
R=pkgroot
rm -rf "$R" out-mac; mkdir -p "$R/Library/Audio/Plug-Ins/VST3" "$R/Library/Audio/Plug-Ins/Components" "$R/Users/Shared/DEMOCRAZY" out-mac
PLUGINS=("TeaTowel|Tea Towel" "TapeTwin|Tape Twin" "Lathe|Lathe" "Schaltwerk|Schaltwerk" "Loon|Loon" "Carousel|Carousel" "Undertow|Undertow" "Parlour|Parlour" "Ether|Ether" "Chamber|Chamber" "Jangle|Jangle" "Harmonia|Harmonia" "Bandleader|Bandleader")
for p in "${PLUGINS[@]}"; do
  t="${p%%|*}"; n="${p##*|}"
  cp -R "build/${t}_artefacts/Release/VST3/${n}.vst3" "$R/Library/Audio/Plug-Ins/VST3/"
  cp -R "build/${t}_artefacts/Release/AU/${n}.component" "$R/Library/Audio/Plug-Ins/Components/"
done
cp -R Grooves "$R/Users/Shared/DEMOCRAZY/"
SIGN="-"
if [ -n "$MACOS_CERT_P12" ]; then
  echo "$MACOS_CERT_P12" | base64 --decode > cert.p12
  security create-keychain -p ci build.keychain
  security default-keychain -s build.keychain
  security unlock-keychain -p ci build.keychain
  security import cert.p12 -k build.keychain -P "$MACOS_CERT_PASSWORD" -T /usr/bin/codesign -T /usr/bin/productsign
  security set-key-partition-list -S apple-tool:,apple: -s -k ci build.keychain
  SIGN="$MACOS_SIGN_ID"
fi
for b in "$R"/Library/Audio/Plug-Ins/VST3/*.vst3 "$R"/Library/Audio/Plug-Ins/Components/*.component; do
  if [ "$SIGN" = "-" ]; then codesign --force --deep -s - "$b"; else codesign --force --deep --options runtime --timestamp -s "$SIGN" "$b"; fi
done
pkgbuild --analyze --root "$R" comp.plist
# keep every plugin where it's put (not "relocated" to an old copy somewhere else on the disk)
i=0
while /usr/libexec/PlistBuddy -c "Print :$i" comp.plist >/dev/null 2>&1; do
  /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" comp.plist 2>/dev/null || /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" comp.plist || true
  i=$((i+1))
done
pkgbuild --root "$R" --component-plist comp.plist --identifier com.democrazy.suite --version "$VERSION" --install-location / democrazy-component.pkg
productbuild --package democrazy-component.pkg "out-mac/DEMOCRAZY Installer (Mac).pkg"
if [ -n "$MACOS_INSTALLER_SIGN_ID" ]; then
  productsign --sign "$MACOS_INSTALLER_SIGN_ID" "out-mac/DEMOCRAZY Installer (Mac).pkg" "out-mac/signed.pkg" && mv "out-mac/signed.pkg" "out-mac/DEMOCRAZY Installer (Mac).pkg"
fi
if [ -n "$MACOS_NOTARY_ID" ]; then
  xcrun notarytool submit "out-mac/DEMOCRAZY Installer (Mac).pkg" --apple-id "$MACOS_NOTARY_ID" --password "$MACOS_NOTARY_PASSWORD" --team-id "$MACOS_TEAM_ID" --wait
  xcrun stapler staple "out-mac/DEMOCRAZY Installer (Mac).pkg"
fi
