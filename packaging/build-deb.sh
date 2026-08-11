#!/bin/bash
# ============================================================
# build-deb.sh — Build a minimal .deb package for system-Qt users
#
# Usage:
#   ./build-deb.sh <app-binary-path> <version> <arch> <output-dir>
#
# Example:
#   ./build-deb.sh build/SerialDebug 2.0.1 amd64 build/deploy
#
# The .deb declares Depends on system Qt5 packages (NOT bundled).
# No external dependencies beyond dpkg-deb and coreutils.
# ============================================================
set -euo pipefail

BINARY="${1:?Usage: $0 <binary> <version> <arch> <outdir>}"
VERSION="${2:?}"
ARCH="${3:?}"
OUTDIR="${4:?}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEB_ROOT="${OUTDIR}/deb-pkg"
PKG_NAME="serial-debug"
DEB_FILE="${OUTDIR}/${PKG_NAME}_${VERSION}_${ARCH}.deb"

# Clean
rm -rf "$DEB_ROOT"
mkdir -p "$DEB_ROOT"

# Copy DEBIAN control files (with version/arch substituted)
mkdir -p "$DEB_ROOT/DEBIAN"
cp -r "$SCRIPT_DIR/deb/DEBIAN/"* "$DEB_ROOT/DEBIAN/"
sed -i "s/Version: 2.0.0/Version: ${VERSION}/" "$DEB_ROOT/DEBIAN/control"
sed -i "s/Architecture: amd64/Architecture: ${ARCH}/" "$DEB_ROOT/DEBIAN/control"
chmod 755 "$DEB_ROOT/DEBIAN/postinst"

# Copy binary
mkdir -p "$DEB_ROOT/usr/bin"
cp "$BINARY" "$DEB_ROOT/usr/bin/SerialDebug"
chmod 755 "$DEB_ROOT/usr/bin/SerialDebug"

# Copy desktop entry
mkdir -p "$DEB_ROOT/usr/share/applications"
cp "$SCRIPT_DIR/deb/usr/share/applications/serial-debug.desktop" \
   "$DEB_ROOT/usr/share/applications/"

# Copy udev rules
mkdir -p "$DEB_ROOT/etc/udev/rules.d"
cp "$SCRIPT_DIR/deb/etc/udev/rules.d/99-serial-debug.rules" \
   "$DEB_ROOT/etc/udev/rules.d/"

# Create a scalable SVG icon (no ImageMagick needed)
mkdir -p "$DEB_ROOT/usr/share/icons/hicolor/scalable/apps"
cat > "$DEB_ROOT/usr/share/icons/hicolor/scalable/apps/serial-debug.svg" << 'SVGEOF'
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

# Copy doc
cp "$SCRIPT_DIR/../README.md" "$DEB_ROOT/usr/share/doc/serial-debug/README" 2>/dev/null || true

# Build the .deb (dpkg-deb works without root for building)
dpkg-deb --build "$DEB_ROOT" "$DEB_FILE"

echo "=== .deb created: ${DEB_FILE} ==="
echo "Size: $(du -h "$DEB_FILE" | cut -f1)"
echo ""
echo "Contents:"
dpkg --contents "$DEB_FILE" 2>/dev/null | head -30 || true