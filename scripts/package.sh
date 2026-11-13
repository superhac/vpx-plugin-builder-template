#!/usr/bin/env bash
# Build + stage a ready-to-distribute plugin folder, then zip it.
#
# Usage: scripts/package.sh <os>-<arch>
#   e.g. scripts/package.sh linux-x64
#        scripts/package.sh macos-universal
#        scripts/package.sh windows-x64
#
# Produces:  dist/<PLUGIN_ID>/            (folder to drop into VPX plugins dir)
#            dist/<id lowercase>-<os>-<arch>.zip (the store download)
set -euo pipefail

SLUG="${1:?usage: package.sh <os>-<arch>  (e.g. linux-x64)}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
STAGE="$ROOT/dist"

# Everything is named from the `id` in plugin.cfg.
PLUGIN_ID="$(sed -n 's/^id[[:space:]]*=[[:space:]]*"\(.*\)"/\1/p' "$ROOT/plugin.cfg")"
ZIP="$(echo "$PLUGIN_ID" | tr '[:upper:]' '[:lower:]')-${SLUG}.zip"

rm -rf "$BUILD" "$STAGE"
cmake -B "$BUILD" -S "$ROOT" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --config Release
cmake --install "$BUILD" --prefix "$STAGE" --config Release

( cd "$STAGE" && zip -r "$ZIP" "$PLUGIN_ID" )
echo "Packaged: $STAGE/$ZIP"
ls -R "$STAGE/$PLUGIN_ID"
