#!/usr/bin/env bash
set -euo pipefail

# Build and package Cool Live Caption as an AppImage on Linux.
# Requirements:
# - build-essential / clang / ninja
# - libpipewire-0.3-dev (for audio capture)
# - fuse/appimagetool runtime support to run AppImages

ROOT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="${ROOT_DIR}/build-appimage"
APPDIR="${BUILD_DIR}/coollivecaptions.AppDir"
APPNAME="Cool Live Caption"
BIN_NAME="coollivecaptions"
ICON_SRC_PNG="${ROOT_DIR}/resources/icon-appimage.png"
DESKTOP_FILE="${APPDIR}/usr/share/applications/${BIN_NAME}.desktop"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --target "${BIN_NAME}"

rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin" "${APPDIR}/usr/share/applications" "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

cp "${BUILD_DIR}/bin/${BIN_NAME}" "${APPDIR}/usr/bin/"

# Bundle runtime siblings that our CMake copies next to the binary (onnxruntime, april-asr .so, profanity lists).
if compgen -G "${BUILD_DIR}/bin/*.so" > /dev/null; then
	cp "${BUILD_DIR}/bin"/*.so "${APPDIR}/usr/bin/"
fi
if [ -d "${ROOT_DIR}/resources/profanity" ]; then
	cp -r "${ROOT_DIR}/resources/profanity" "${APPDIR}/usr/bin/"
fi

cat > "${DESKTOP_FILE}" <<EOF
[Desktop Entry]
Type=Application
Name=Cool Live Captions
Exec=${BIN_NAME}
Icon=${BIN_NAME}
Categories=AudioVideo;Utility;
Terminal=false
EOF

if [ -f "${ICON_SRC_PNG}" ]; then
	cp "${ICON_SRC_PNG}" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${BIN_NAME}.png"
else
	echo "warning: no icon found at ${ICON_SRC_PNG}"
fi

LINUXDEPLOY="${BUILD_DIR}/linuxdeploy-x86_64.AppImage"
APPIMAGETOOL="${BUILD_DIR}/appimagetool-x86_64.AppImage"

if [ ! -x "${LINUXDEPLOY}" ]; then
	curl -L "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" -o "${LINUXDEPLOY}"
	chmod +x "${LINUXDEPLOY}"
fi

if [ ! -x "${APPIMAGETOOL}" ]; then
	curl -L "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" -o "${APPIMAGETOOL}"
	chmod +x "${APPIMAGETOOL}"
fi

export APPIMAGE_EXTRACT_AND_RUN=1

"${LINUXDEPLOY}" \
	--appdir "${APPDIR}" \
	--executable "${APPDIR}/usr/bin/${BIN_NAME}" \
	--desktop-file "${DESKTOP_FILE}" \
	--icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${BIN_NAME}.png" \
	--exclude-library "libpulse*" \
	--exclude-library "libpipewire-0.3*"

"${APPIMAGETOOL}" "${APPDIR}" "${BUILD_DIR}/${APPNAME}.AppImage"

echo "AppImage created at: ${BUILD_DIR}/${APPNAME}.AppImage"
