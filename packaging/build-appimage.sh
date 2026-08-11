#!/bin/bash
# ============================================================
# build-appimage.sh — Build a self-contained AppImage
#
# Usage:
#   ./build-appimage.sh <app-binary-path> <version> <output-dir>
#
# Example:
#   ./build-appimage.sh build/SerialDebug 2.0.0 build/deploy
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
APPIMAGE_FILE="${OUTDIR}/SerialDebug-${VERSION}-x86_64.AppImage"

# Clean
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/usr/bin"
mkdir -p "$APP_DIR/usr/share/applications"
mkdir -p "$APP_DIR/usr/share/icons/hicolor/256x256/apps"

# Copy binary
cp "$BINARY" "$APP_DIR/usr/bin/SerialDebug"
chmod 755 "$APP_DIR/usr/bin/SerialDebug"

# Copy desktop file
cp "$SCRIPT_DIR/deb/usr/share/applications/serial-debug.desktop" \
   "$APP_DIR/usr/share/applications/"

# Copy icon (must match Icon=serial-debug in the desktop file)
# linuxdeploy errors out if it cannot find a suitable icon. We ship an SVG
# under the scalable dir (same as the deb package) — rsvg/inkscape-compatible
# converters may not be installed, so ensure the exact name resolves.
ICON_SVG_DIR="$APP_DIR/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$ICON_SVG_DIR"
cat > "$ICON_SVG_DIR/serial-debug.svg" << 'SVGEOF'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
  <rect width="256" height="256" rx="32" fill="#1a1b26"/>
  <g transform="translate(128,128)" fill="none" stroke="#7aa2f7" stroke-width="8" stroke-linecap="round">
    <rect x="-72" y="-40" width="144" height="80" rx="8"/>
    <rect x="-48" y="-24" width="96" height="48" rx="4"/>
    <line x1="-48" y1="0" x2="48" y2="0"/>
    <line x1="0" y1="-24" x2="0" y2="24"/>
    <line x1="-48" y1="-12" x2="-12" y2="-12"/>
    <line x1="-48" y1="12" x2="-12" y2="12"/>
    <line x1="12" y1="-12" x2="48" y2="-12"/>
    <line x1="12" y1="12" x2="48" y2="12"/>
    <circle cx="0" cy="0" r="8" fill="#9ece6a" stroke="none"/>
  </g>
</svg>
SVGEOF

# Also generate a PNG if a converter is available (linuxdeploy prefers PNG)
ICON_DIR="$APP_DIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$ICON_DIR"
if command -v rsvg-convert &>/dev/null; then
    rsvg-convert -w 256 -h 256 "$ICON_SVG_DIR/serial-debug.svg" -o "$ICON_DIR/serial-debug.png" 2>/dev/null || true
elif command -v convert &>/dev/null; then
    convert "$ICON_SVG_DIR/serial-debug.svg" -resize 256x256 "$ICON_DIR/serial-debug.png" 2>/dev/null || true
elif command -v inkscape &>/dev/null; then
    inkscape "$ICON_SVG_DIR/serial-debug.svg" --export-type=png --export-filename="$ICON_DIR/serial-debug.png" 2>/dev/null || true
fi
ls -la "$ICON_DIR" 2>/dev/null || true
ls -la "$ICON_SVG_DIR" 2>/dev/null || true

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
elif [ -n "$(find "${OUTDIR}" -maxdepth 1 -name 'SerialDebug-*.AppImage' -type f 2>/dev/null)" ]; then
    # linuxdeploy sometimes writes to a different name; pick it up
    real_img="$(find "${OUTDIR}" -maxdepth 1 -name 'SerialDebug-*.AppImage' -type f 2>/dev/null | head -1)"
    mv "$real_img" "${APPIMAGE_FILE}"
    echo "✅ AppImage created (renamed): ${APPIMAGE_FILE}"
    echo "   Size: $(du -h "${APPIMAGE_FILE}" | cut -f1)"
else
    echo "ERROR: AppImage output not found."
    find "${OUTDIR}" -name "*.AppImage" -type f 2>/dev/null
    exit 1
fi