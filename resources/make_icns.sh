#!/bin/sh
# Builds a .icns from a single source image (any format sips reads, incl. .ico) by resampling it
# into a standard iconset. Source is 256x256 at best, so the 512/1024 tiers are upscaled - soft but
# fine for how small these render (Finder icons, file-type icons), not worth a design pass.
set -e

SRC="$1"
OUT="$2"

if [ -z "$SRC" ] || [ -z "$OUT" ]; then
    echo "Usage: make_icns.sh <source-image> <output.icns>" >&2
    exit 1
fi

WORKDIR=$(mktemp -d)
ICONSET="$WORKDIR/icon.iconset"
mkdir -p "$ICONSET"
trap 'rm -rf "$WORKDIR"' EXIT

resize() {
    sips -s format png -z "$1" "$1" "$SRC" --out "$ICONSET/$2" >/dev/null 2>&1
}

resize 16   icon_16x16.png
resize 32   icon_16x16@2x.png
resize 32   icon_32x32.png
resize 64   icon_32x32@2x.png
resize 128  icon_128x128.png
resize 256  icon_128x128@2x.png
resize 256  icon_256x256.png
resize 512  icon_256x256@2x.png
resize 512  icon_512x512.png
resize 1024 icon_512x512@2x.png

iconutil -c icns "$ICONSET" -o "$OUT"
