#!/usr/bin/env bash
set -euo pipefail

# Build a local Flatpak using flatpak-builder (no Flathub upload).
# Optional env: DO_INSTALL=1 to install into user collection after build.

ROOT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
APP_ID="com.batterydie.coollivecaptions"
RUNTIME="org.freedesktop.Platform"
SDK="org.freedesktop.Sdk"
BRANCH="23.08"
BUILD_DIR="${ROOT_DIR}/build/flatpak"
APPDIR="${BUILD_DIR}/appdir"
MANIFEST="${BUILD_DIR}/${APP_ID}.yml"
ICON_SRC="${ROOT_DIR}/resources/icon-appimage.png"
DESKTOP_SRC="${ROOT_DIR}/resources/${APP_ID}.desktop"
APPDATA_SRC="${ROOT_DIR}/resources/${APP_ID}.appdata.xml"
NO_SANDBOX=${NO_SANDBOX:-0}

mkdir -p "${BUILD_DIR}"

# Ensure desktop and appdata exist (generate minimal ones if absent).
if [[ ! -f "${DESKTOP_SRC}" ]]; then
  cat > "${DESKTOP_SRC}" <<'EOF'
[Desktop Entry]
Type=Application
Name=Cool Live Captions
Comment=FOSS desktop live captioning application
Exec=coollivecaptions
Icon=com.batterydie.coollivecaptions
Terminal=false
Categories=AudioVideo;Utility;
EOF
fi

if [[ ! -f "${APPDATA_SRC}" ]]; then
  cat > "${APPDATA_SRC}" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop">
  <id>com.batterydie.coollivecaptions</id>
  <name>Cool Live Captions</name>
  <summary>FOSS desktop live captioning application</summary>
  <metadata_license>CC0-1.0</metadata_license>
  <project_license>GPL-3.0</project_license>
  <developer_name>Luca Jones</developer_name>
  <url type="homepage">https://github.com/BatteryDie/Cool-Live-Captions</url>
  <launchable type="desktop-id">com.batterydie.coollivecaptions.desktop</launchable>
  <provides>
    <binary>coollivecaptions</binary>
  </provides>
</component>
EOF
fi

cat > "${MANIFEST}" <<EOF
app-id: ${APP_ID}
runtime: ${RUNTIME}
runtime-version: '${BRANCH}'
sdk: ${SDK}
command: coollivecaptions
build-options:
  build-args:
    - --share=network
finish-args:
  - --share=network
  - --socket=pulseaudio
  # PipeWire portal access
  - --socket=session-bus
  - --socket=wayland
  - --socket=x11
  - --device=dri
  - --talk-name=org.freedesktop.portal.Desktop
  - --env=LD_LIBRARY_PATH=/app/lib/coollivecaptions:/app/lib
  - --filesystem=home/.config/CoolLiveCaptions:create
  - --filesystem=home/.coollivecaptions:create
  - --filesystem=xdg-documents/Cool Live Captions:create
  - --filesystem=xdg-run/pipewire-0
modules:
  - name: coollivecaptions
    buildsystem: simple
    build-commands:
      - cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX=/app
      - cmake --build build --parallel
      - install -d /app/lib/coollivecaptions
      - install -Dm755 build/bin/coollivecaptions /app/bin/coollivecaptions
      - find build -maxdepth 5 -name 'libaprilasr.so*' -exec install -Dm755 {} /app/lib/coollivecaptions/ \;
      - if compgen -G "build/bin/*.so" > /dev/null; then install -Dm755 build/bin/*.so /app/lib/coollivecaptions/; fi
      - ln -sf libonnxruntime.so /app/lib/coollivecaptions/libonnxruntime.so.1
      - if [ -d build/bin/profanity ]; then mkdir -p /app/lib/coollivecaptions/profanity && cp -r build/bin/profanity/* /app/lib/coollivecaptions/profanity/; fi
      - if [ -d /app/lib/coollivecaptions/profanity ]; then ln -sfn /app/lib/coollivecaptions/profanity /app/bin/profanity; fi
      - install -Dm644 resources/icon-appimage.png /app/share/icons/hicolor/256x256/apps/com.batterydie.coollivecaptions.png
      - install -Dm644 resources/${APP_ID}.desktop /app/share/applications/${APP_ID}.desktop
      - install -Dm644 resources/${APP_ID}.appdata.xml /app/share/metainfo/${APP_ID}.appdata.xml
    sources:
      - type: dir
        path: ${ROOT_DIR}
EOF

BUILDER_FLAGS=()
if [[ "${NO_SANDBOX}" -eq 1 ]]; then
  BUILDER_FLAGS+=(--disable-sandbox)
fi

flatpak-builder "${BUILDER_FLAGS[@]}" --force-clean "${APPDIR}" "${MANIFEST}"

if [[ "${DO_INSTALL:-0}" -eq 1 ]]; then
  flatpak-builder "${BUILDER_FLAGS[@]}" --user --install --force-clean "${APPDIR}" "${MANIFEST}"
fi

echo "Flatpak build dir: ${APPDIR}"; echo "To run locally: flatpak-builder --run ${APPDIR} ${MANIFEST} coollivecaptions"; echo "To install locally: DO_INSTALL=1 ./deploy/linux-flatpak.sh"
