#!/usr/bin/env bash
set -euo pipefail

# Build PyFeiQ Fluent AppImage on Ubuntu 20.04
# Usage: bash build_appimage.sh

APPDIR="$(pwd)/AppDir"
TOOLS_DIR="$(pwd)/.appimage-tools"
ICON_DIR="$(pwd)/icons"

mkdir -p "$TOOLS_DIR" "$ICON_DIR"

# Prepare icon: convert existing bmp to png if no icon provided
if [ ! -f "$ICON_DIR/kylin.png" ]; then
  SRC_BMP="Chatroom/resources/page.bmp"
  if command -v convert >/dev/null 2>&1; then
    convert "$SRC_BMP" -resize 256x256 "$ICON_DIR/kylin.png"
  else
    echo "ImageMagick 'convert' not found, installing..." >&2
    sudo apt-get update && sudo apt-get install -y imagemagick
    convert "$SRC_BMP" -resize 256x256 "$ICON_DIR/kylin.png"
  fi
fi

pushd "$TOOLS_DIR" >/dev/null
  if [ ! -f linuxdeploy ]; then
    wget -q -O linuxdeploy https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x linuxdeploy
  fi
  if [ ! -f linuxdeploy-plugin-python ]; then
    wget -q -O linuxdeploy-plugin-python https://github.com/linuxdeploy/linuxdeploy-plugin-python/releases/download/continuous/linuxdeploy-plugin-python-x86_64.AppImage
    chmod +x linuxdeploy-plugin-python
  fi
  if [ ! -f linuxdeploy-plugin-qt ]; then
    wget -q -O linuxdeploy-plugin-qt https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
    chmod +x linuxdeploy-plugin-qt
  fi
  if [ ! -f appimagetool ]; then
    wget -q -O appimagetool https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x appimagetool
  fi
popd >/dev/null

rm -rf "$APPDIR"

# Ensure AppRun is executable
chmod +x AppRun

# Build with linuxdeploy, bundle python and qt
APPDIR="$APPDIR" \
"$TOOLS_DIR/linuxdeploy" \
  --appdir "$APPDIR" \
  -d kylin.desktop \
  -i "$ICON_DIR/kylin.png" \
  --plugin qt \
  --plugin python \
  --output appimage \
  -- \
  --python-reqs-file requirements.appimage.txt

# In case linuxdeploy didn't emit the AppImage, do it manually
if ! ls ./*.AppImage >/dev/null 2>&1; then
  "$TOOLS_DIR/appimagetool" "$APPDIR"
fi

echo "Build finished. Output AppImage:"
ls -lh ./*.AppImage

