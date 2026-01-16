#!/usr/bin/env bash
set -euo pipefail

# Build a .deb package from an existing Release build.
# Defaults to build-linux-release; override with -b/--build-dir. Set version with -v/--version.

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
BUILD_DIR_DEFAULT="${ROOT_DIR}/build-linux-release"
BUILD_DIR=${BUILD_DIR:-"${BUILD_DIR_DEFAULT}"}
VERSION=${VERSION:-"0.1.0"}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -b|--build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    -v|--version)
      VERSION="$2"
      shift 2
      ;;
    -h|--help)
      echo "Usage: [-b build_dir] [-v version]" >&2
      echo "Default build dir: ${BUILD_DIR_DEFAULT}" >&2
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done
PKG_ROOT="${ROOT_DIR}/build/deb/pkg-root"
OUT_DIR="${ROOT_DIR}/build/deb"
ARCH=amd64
PKG_NAME=cool-live-captions
BIN_NAME=coollivecaptions
BIN_SRC="${BUILD_DIR}/bin/${BIN_NAME}"
LIB_SRC_DIR="${BUILD_DIR}/bin"
DESKTOP_SRC="${ROOT_DIR}/resources/com.batterydie.coollivecaptions.desktop"
APPDATA_SRC="${ROOT_DIR}/resources/com.batterydie.coollivecaptions.appdata.xml"
ICON_SRC="${ROOT_DIR}/resources/icon-appimage.png"

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

# Copy binary and add a hyphenated symlink for convenience
cp "${BIN_SRC}" "${PKG_ROOT}/usr/bin/${BIN_NAME}"
ln -sf "${BIN_NAME}" "${PKG_ROOT}/usr/bin/${PKG_NAME}"

# Copy private libs (onnx/april-asr if present) and profanity lists
if compgen -G "${LIB_SRC_DIR}/*.so" > /dev/null; then
  cp "${LIB_SRC_DIR}"/*.so "${PKG_ROOT}/usr/lib/${PKG_NAME}/"
fi
if [[ -d "${LIB_SRC_DIR}/profanity" ]]; then
  cp -r "${LIB_SRC_DIR}/profanity" "${PKG_ROOT}/usr/lib/${PKG_NAME}/"
fi

# Desktop entry (generate a simple one if missing in repo)
DESKTOP_TARGET="${PKG_ROOT}/usr/share/applications/com.batterydie.coollivecaptions.desktop"
if [[ -f "${DESKTOP_SRC}" ]]; then
  install -Dm644 "${DESKTOP_SRC}" "${DESKTOP_TARGET}"
else
  cat > "${DESKTOP_TARGET}" <<EOF
[Desktop Entry]
Type=Application
Name=Cool Live Captions
Comment=FOSS desktop live captioning application
Exec=${BIN_NAME}
Icon=com.batterydie.coollivecaptions
Terminal=false
Categories=Utility;AudioVideo;
EOF
fi

# AppStream metadata (generate minimal metadata if missing)
APPDATA_TARGET="${PKG_ROOT}/usr/share/metainfo/com.batterydie.coollivecaptions.appdata.xml"
if [[ -f "${APPDATA_SRC}" ]]; then
  install -Dm644 "${APPDATA_SRC}" "${APPDATA_TARGET}"
else
  cat > "${APPDATA_TARGET}" <<EOF
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
    <binary>${BIN_NAME}</binary>
  </provides>
  <releases>
    <release version="${VERSION}"/>
  </releases>
</component>
EOF
fi

# Icon
if [[ -f "${ICON_SRC}" ]]; then
  install -Dm644 "${ICON_SRC}" "${PKG_ROOT}/usr/share/icons/hicolor/256x256/apps/com.batterydie.coollivecaptions.png"
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
Description: FOSS desktop live captioning application
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
