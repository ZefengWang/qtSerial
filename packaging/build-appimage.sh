#!/bin/bash
# ============================================================
# build-appimage.sh — Build a self-contained AppImage
#
# Usage:
#   ./build-appimage.sh <app-binary-path> <version> <output-dir>
#
# Example:
#   ./build-appimage.sh build/serial-debug 2.0.0 build/deploy
#
# Uses linuxdeploy + linuxdeploy-plugin-qt to bundle all Qt
# and system dependencies into a portable AppImage.
# ============================================================
set -euo pipefail

BINARY="${1:?Usage: $0 <binary> <version> <outdir>}"
VERSION="${2:?}"
OUTDIR="${3:?}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="${OUTDIR}/AppDir"
APPIMAGE_FILE="${OUTDIR}/serial-debug-${VERSION}.AppImage"

# Clean
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/usr/bin"
mkdir -p "$APP_DIR/usr/share/applications"
mkdir -p "$APP_DIR/usr/share/icons/hicolor/256x256/apps"

# Copy binary
cp "$BINARY" "$APP_DIR/usr/bin/serial-debug"
chmod 755 "$APP_DIR/usr/bin/serial-debug"

# Copy desktop file
cp "$SCRIPT_DIR/deb/usr/share/applications/serial-debug.desktop" \
   "$APP_DIR/usr/share/applications/"

# Copy icon (must match Icon=serial-debug in the desktop file)
ICON_SRC_DIR="$SCRIPT_DIR/../icon"
SCALABLE_DIR="$APP_DIR/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$SCALABLE_DIR"
cp "$ICON_SRC_DIR/serial-debug.svg" "$SCALABLE_DIR/serial-debug.svg"
ICON_DIR="$APP_DIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$ICON_DIR"
if [ -f "$ICON_SRC_DIR/serial-debug.png" ]; then
    cp "$ICON_SRC_DIR/serial-debug.png" "$ICON_DIR/serial-debug.png"
fi
ls -la "$ICON_DIR" 2>/dev/null || true
ls -la "$SCALABLE_DIR" 2>/dev/null || true

# Download linuxdeploy if not cached
LINUXDEPLOY="${OUTDIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${OUTDIR}/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "$LINUXDEPLOY" ]; then
    echo "Downloading linuxdeploy..."
    wget -q "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" -O "$LINUXDEPLOY"
    chmod +x "$LINUXDEPLOY"
fi
if [ ! -f "$LINUXDEPLOY_QT" ]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    wget -q "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" -O "$LINUXDEPLOY_QT"
    chmod +x "$LINUXDEPLOY_QT"
fi

# Set up environment for linuxdeploy
export LDAI_OUTPUT="${APPIMAGE_FILE}"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}"

# Run linuxdeploy with Qt plugin
echo "Running linuxdeploy..."
# We need to disable the AppImage output check for FUSE-less environments
# and use --appimage-extract-and-run for the same reason
LINUXDEPLOY_BIN="$LINUXDEPLOY"
if [ ! -c /dev/fuse ]; then
    echo "  (no FUSE, using extract mode)"
    LINUXDEPLOY_BIN="${OUTDIR}/linuxdeploy-extracted"
    if [ ! -f "$LINUXDEPLOY_BIN/AppRun" ]; then
        mkdir -p "$LINUXDEPLOY_BIN"
        cd "$LINUXDEPLOY_BIN"
        "$LINUXDEPLOY" --appimage-extract >/dev/null 2>&1
        cd - >/dev/null
    fi
    LINUXDEPLOY_BIN="${LINUXDEPLOY_BIN}/squashfs-root/AppRun"
fi

# Run linuxdeploy
"$LINUXDEPLOY_BIN" \
    --appdir "$APP_DIR" \
    --plugin qt \
    --output appimage \
    2>&1 | tail -30

# Check if AppImage was produced
if [ -f "${APPIMAGE_FILE}" ]; then
    echo "✅ AppImage created: ${APPIMAGE_FILE}"
    echo "   Size: $(du -h "${APPIMAGE_FILE}" | cut -f1)"
elif [ -n "$(find "${OUTDIR}" -maxdepth 1 -name 'serial-debug-*.AppImage' -type f 2>/dev/null)" ]; then
    # linuxdeploy sometimes writes to a different name; pick it up
    real_img="$(find "${OUTDIR}" -maxdepth 1 -name 'serial-debug-*.AppImage' -type f 2>/dev/null | head -1)"
    mv "$real_img" "${APPIMAGE_FILE}"
    echo "✅ AppImage created (renamed): ${APPIMAGE_FILE}"
    echo "   Size: $(du -h "${APPIMAGE_FILE}" | cut -f1)"
else
    echo "ERROR: AppImage output not found."
    find "${OUTDIR}" -name "*.AppImage" -type f 2>/dev/null
    exit 1
fi