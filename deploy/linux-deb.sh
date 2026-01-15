#!/usr/bin/env bash
set -euo pipefail

# Build a .deb package from an existing Release build.
# Usage: BUILD_DIR=build-linux-release VERSION=0.1.0 ./deploy/build-deb.sh

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"${ROOT_DIR}/build-linux-release"}
VERSION=${VERSION:-"0.1.0"}
PKG_ROOT="${ROOT_DIR}/build/deb/pkg-root"
OUT_DIR="${ROOT_DIR}/build/deb"
ARCH=amd64
PKG_NAME=cool-live-caption
BIN_SRC="${BUILD_DIR}/bin/coollivecaption"
LIB_SRC_DIR="${BUILD_DIR}/bin"
DESKTOP_SRC="${ROOT_DIR}/resources/flatpak/com.batterydie.CoolLiveCaption.desktop"
APPDATA_SRC="${ROOT_DIR}/resources/flatpak/com.batterydie.CoolLiveCaption.appdata.xml"
ICON_SRC="${ROOT_DIR}/resources/icon.png"

if [[ ! -x "${BIN_SRC}" ]]; then
  echo "error: binary not found at ${BIN_SRC}. Build first." >&2
  exit 1
fi

rm -rf "${PKG_ROOT}"
mkdir -p "${PKG_ROOT}/DEBIAN" \
         "${PKG_ROOT}/usr/bin" \
         "${PKG_ROOT}/usr/lib/${PKG_NAME}" \
         "${PKG_ROOT}/usr/share/applications" \
         "${PKG_ROOT}/usr/share/icons/hicolor/256x256/apps" \
         "${PKG_ROOT}/usr/share/metainfo"

# Copy binary
cp "${BIN_SRC}" "${PKG_ROOT}/usr/bin/${PKG_NAME}"

# Copy private libs (onnx/april-asr if present) and profanity lists
if compgen -G "${LIB_SRC_DIR}/*.so" > /dev/null; then
  cp "${LIB_SRC_DIR}"/*.so "${PKG_ROOT}/usr/lib/${PKG_NAME}/"
fi
if [[ -d "${LIB_SRC_DIR}/profanity" ]]; then
  cp -r "${LIB_SRC_DIR}/profanity" "${PKG_ROOT}/usr/lib/${PKG_NAME}/"
fi

# Desktop entry
if [[ -f "${DESKTOP_SRC}" ]]; then
  install -Dm644 "${DESKTOP_SRC}" "${PKG_ROOT}/usr/share/applications/com.batterydie.CoolLiveCaption.desktop"
fi

# AppStream metadata
if [[ -f "${APPDATA_SRC}" ]]; then
  install -Dm644 "${APPDATA_SRC}" "${PKG_ROOT}/usr/share/metainfo/com.batterydie.CoolLiveCaption.appdata.xml"
fi

# Icon
if [[ -f "${ICON_SRC}" ]]; then
  install -Dm644 "${ICON_SRC}" "${PKG_ROOT}/usr/share/icons/hicolor/256x256/apps/com.batterydie.CoolLiveCaption.png"
fi

# Control file
cat > "${PKG_ROOT}/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Depends: libc6 (>= 2.31), libstdc++6, libgcc-s1, libgl1, libx11-6, libxrandr2, libpulse0, libpipewire-0.3-0
Maintainer: Luca Jones <>
Description: Cool Live Caption - on-device live captions for desktop audio
EOF

# Postinst to refresh desktop databases (best effort)
cat > "${PKG_ROOT}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database >/usr/share/applications || true
fi
if command -v update-icon-caches >/dev/null 2>&1; then
  update-icon-caches /usr/share/icons/hicolor || true
fi
exit 0
EOF
chmod 0755 "${PKG_ROOT}/DEBIAN/postinst"

mkdir -p "${OUT_DIR}"
dpkg-deb --build "${PKG_ROOT}" "${OUT_DIR}/${PKG_NAME}_${VERSION}_${ARCH}.deb"
echo "Built deb: ${OUT_DIR}/${PKG_NAME}_${VERSION}_${ARCH}.deb"
