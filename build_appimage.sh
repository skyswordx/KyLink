#!/usr/bin/env bash
set -euo pipefail
# uncomment for debugging by running: DEBUG=1 bash build_appimage.sh
if [ "${DEBUG:-0}" != "0" ]; then set -x; fi

# Build PyFeiQ Fluent AppImage on Ubuntu 20.04
# Usage: bash build_appimage.sh

SUDO=""
if [ "${EUID:-$(id -u)}" -ne 0 ]; then
  if command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
  fi
fi

APT_GET_CMD=(apt-get)
if [ -n "$SUDO" ]; then
  APT_GET_CMD=("$SUDO" apt-get)
fi

ensure_deps() {
  local missing=()
  local pkg
  for pkg in "$@"; do
    if ! dpkg -s "$pkg" >/dev/null 2>&1; then
      missing+=("$pkg")
    fi
  done
  if [ ${#missing[@]} -gt 0 ]; then
    echo "[INFO] Installing missing system packages: ${missing[*]}"
    "${APT_GET_CMD[@]}" update
    "${APT_GET_CMD[@]}" install -y "${missing[@]}"
  fi
}

PROJ_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJ_DIR"

APPDIR="$PROJ_DIR/AppDir"
TOOLS_CACHE_DIR="$PROJ_DIR/.appimage-tools"
ICON_DIR="$PROJ_DIR/icons"
TOOLS_RUNTIME_DIR="$(mktemp -d -p "${TMPDIR:-/tmp}" kylin-appimage-tools-XXXXXX)"
cleanup() {
  rm -rf "$TOOLS_RUNTIME_DIR"
}
trap cleanup EXIT

ensure_deps libfuse2 squashfs-tools desktop-file-utils patchelf zsync

mkdir -p "$TOOLS_CACHE_DIR" "$ICON_DIR"

PIP_REQUIREMENTS_FILE="$PROJ_DIR/requirements.appimage.txt"
if [ -z "${PIP_REQUIREMENTS:-}" ] && [ -f "$PIP_REQUIREMENTS_FILE" ]; then
  export PIP_REQUIREMENTS="-r $PIP_REQUIREMENTS_FILE"
fi

# Prepare icon: convert existing bmp to png if available, otherwise create a placeholder
if [ ! -f "$ICON_DIR/kylin.png" ]; then
  SRC_BMP="$PROJ_DIR/Chatroom/resources/page.bmp"
  if command -v convert >/dev/null 2>&1; then
    if [ -f "$SRC_BMP" ]; then
      convert "$SRC_BMP" -resize 256x256 "$ICON_DIR/kylin.png"
    else
      echo "Icon source not found at $SRC_BMP, generating placeholder icon..."
      convert -size 256x256 xc:'#2b2b2b' -fill '#00bcd4' -gravity center -pointsize 48 -annotate +0+0 'KF' "$ICON_DIR/kylin.png"
    fi
  else
    echo "ImageMagick 'convert' not found, installing..." >&2
    ensure_deps imagemagick
    if [ -f "$SRC_BMP" ]; then
      convert "$SRC_BMP" -resize 256x256 "$ICON_DIR/kylin.png"
    else
      echo "Icon source not found at $SRC_BMP, generating placeholder icon..."
      convert -size 256x256 xc:'#2b2b2b' -fill '#00bcd4' -gravity center -pointsize 48 -annotate +0+0 'KF' "$ICON_DIR/kylin.png"
    fi
  fi
fi

pushd "$TOOLS_CACHE_DIR" >/dev/null
  # Normalize manually downloaded tool names (keep upstream naming convenience)
  declare -A TOOL_ALIASES=(
    ["linuxdeploy"]="linuxdeploy-x86_64.AppImage"
    ["linuxdeploy-plugin-python"]="linuxdeploy-plugin-python-x86_64.AppImage"
    ["linuxdeploy-plugin-qt"]="linuxdeploy-plugin-qt-x86_64.AppImage"
    ["appimagetool"]="appimagetool-x86_64.AppImage"
  )
  for target in "${!TOOL_ALIASES[@]}"; do
    alias_name="${TOOL_ALIASES[$target]}"
    if [ ! -f "$target" ] && [ -f "$alias_name" ]; then
      echo "[INFO] Detected manually provided $alias_name, renaming to $target" >&2
      mv "$alias_name" "$target"
      chmod +x "$target"
    fi
  done

  DL() {
    local out="$1" url="$2"
    local tmp="$out.download" source
    local candidates=(
      "$url"
      "https://mirror.ghproxy.com/$url"
      "https://download.fastgit.org/${url#https://github.com/}"
    )

    for source in "${candidates[@]}"; do
      [ -z "$source" ] && continue
      echo "[INFO] Downloading $out from $source" >&2
      if command -v wget >/dev/null 2>&1; then
        if wget -q -O "$tmp" "$source"; then
          mv "$tmp" "$out"
          chmod +x "$out"
          return 0
        fi
      elif command -v curl >/dev/null 2>&1; then
        if curl -fsSL "$source" -o "$tmp"; then
          mv "$tmp" "$out"
          chmod +x "$out"
          return 0
        fi
      else
        echo "[ERROR] Neither wget nor curl is available for downloading $out" >&2
        return 1
      fi
      rm -f "$tmp"
      echo "[WARN] Failed to download $out from $source" >&2
    done

    rm -f "$tmp"
    echo "[ERROR] Unable to download $out from any mirror. Please check your network or download manually." >&2
    return 1
  }

  if [ ! -f linuxdeploy ]; then
    DL linuxdeploy https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
  fi
  if [ ! -f linuxdeploy-plugin-python ]; then
    DL linuxdeploy-plugin-python https://github.com/linuxdeploy/linuxdeploy-plugin-python/releases/download/continuous/linuxdeploy-plugin-python-x86_64.AppImage
  fi
  if [ ! -f linuxdeploy-plugin-qt ]; then
    DL linuxdeploy-plugin-qt https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
  fi
  if [ ! -f appimagetool ]; then
    DL appimagetool https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
  fi
  if [ ! -f runtime-x86_64 ]; then
    echo "[INFO] Downloading AppImage runtime (64-bit)" >&2
    DL runtime-x86_64 https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64
  fi
popd >/dev/null

# Make runtime copy on an executable filesystem (important for WSL + DrvFS)
for tool in linuxdeploy linuxdeploy-plugin-python linuxdeploy-plugin-qt appimagetool runtime-x86_64; do
  if [ ! -f "$TOOLS_CACHE_DIR/$tool" ]; then
    echo "[ERROR] Expected tool $tool not found in $TOOLS_CACHE_DIR" >&2
    exit 1
  fi
  cp "$TOOLS_CACHE_DIR/$tool" "$TOOLS_RUNTIME_DIR/$tool"
  chmod +x "$TOOLS_RUNTIME_DIR/$tool"
done

export PATH="$TOOLS_RUNTIME_DIR:$PATH"
export LINUXDEPLOY_PLUGIN_DIR="$TOOLS_RUNTIME_DIR"
export APPIMAGE_RUNTIME_FILE="$TOOLS_RUNTIME_DIR/runtime-x86_64"
if [ ! -f "$APPIMAGE_RUNTIME_FILE" ]; then
  echo "[WARN] Local runtime-x86_64 missing, attempting to fetch on-the-fly..." >&2
  if ! DL "$TOOLS_RUNTIME_DIR/runtime-x86_64" https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64; then
    echo "[ERROR] Unable to download AppImage runtime. Please place runtime-x86_64 into $TOOLS_CACHE_DIR manually." >&2
    exit 1
  fi
  chmod +x "$TOOLS_RUNTIME_DIR/runtime-x86_64"
fi

rm -rf "$APPDIR"

mkdir -p "$APPDIR"
echo "[INFO] Staging application sources into AppDir..."
cp -a Chatroom "$APPDIR/"


# Ensure AppRun is executable
if ! chmod +x AppRun >/dev/null 2>&1; then
  echo "[WARN] Failed to mark AppRun as executable. Ensure the repository is on a filesystem that supports chmod (e.g. the WSL ext4 home)." >&2
else
  if [ ! -x AppRun ]; then
    echo "[WARN] AppRun is not executable. WSL Windows drives mounted with noexec will prevent bundling. Consider relocating the project under the WSL home directory." >&2
  fi
fi

# Make plugins discoverable and force extract-run mode for AppImage (WSL has no FUSE)
export APPIMAGE_EXTRACT_AND_RUN=1

# Build with linuxdeploy, bundle python and qt
echo "[INFO] Running linuxdeploy..."
APPDIR="$APPDIR" \
"$TOOLS_RUNTIME_DIR/linuxdeploy" \
  --appdir "$APPDIR" \
  -d kylin.desktop \
  -i "$ICON_DIR/kylin.png" \
  --custom-apprun AppRun \
  --plugin python \
  --output appimage

# In case linuxdeploy didn't emit the AppImage, do it manually
if ! ls ./*.AppImage >/dev/null 2>&1; then
  echo "linuxdeploy didn't emit AppImage, invoking appimagetool manually..."
  "$TOOLS_RUNTIME_DIR/appimagetool" --runtime-file "$APPIMAGE_RUNTIME_FILE" "$APPDIR"
fi

echo "Build finished. Output AppImage:"
ls -lh ./*.AppImage || true
echo "If nothing listed, check $APPDIR contents and rerun with DEBUG=1 for verbose logs."

