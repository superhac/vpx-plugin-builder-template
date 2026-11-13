#!/usr/bin/env bash
# Refresh the vendored plugin SDK headers in sdk/plugins/ from vpinball.
#
# Usage: scripts/update-sdk.sh [git-ref]      (default: master)
#   e.g. scripts/update-sdk.sh
#        scripts/update-sdk.sh v10.8.1
#        scripts/update-sdk.sh 10a52cb0e8aa
#
# Only the header-only *plugin-facing* API headers are copied. Host-side helpers
# (MsgPluginManager, ResURIResolver/Builder, B2SPluginEventStream) need .cpp
# files and third-party deps and are not part of the plugin SDK.
#
# The resolved commit is recorded in sdk/VPINBALL_COMMIT so you always know
# which VPX build the headers match.
set -euo pipefail

REF="${1:-master}"
REPO="https://github.com/vpinball/vpinball.git"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/sdk/plugins"

HEADERS=(
   MsgPlugin.h          # core messaging / settings / threading ABI (required)
   VPXPlugin.h          # VPX events + VPXPluginAPI
   ControllerPlugin.h   # controller / display / audio sources (included by VPXPlugin.h)
   LoggingPlugin.h      # host logging
   ScriptablePlugin.h   # expose scriptable objects to table scripts
   PagedUIPlugin.h      # declarative in-game UI sheets (work in progress upstream)
   PluginStrings.h      # UTF-8 / native path helpers
   ColorSpace.h         # sRGB helpers for display frames
)

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

git clone --quiet --depth 1 --filter=blob:none --sparse --branch "$REF" "$REPO" "$TMP/vpx" 2>/dev/null \
   || { git clone --quiet --filter=blob:none --sparse "$REPO" "$TMP/vpx" && git -C "$TMP/vpx" checkout --quiet "$REF"; }
git -C "$TMP/vpx" sparse-checkout set plugins/plugins

mkdir -p "$DEST"
for h in "${HEADERS[@]}"; do
   cp "$TMP/vpx/plugins/plugins/$h" "$DEST/$h"
done

COMMIT="$(git -C "$TMP/vpx" rev-parse HEAD)"
DATE="$(git -C "$TMP/vpx" log -1 --format=%cs)"
printf '%s %s\n' "$COMMIT" "$DATE" > "$ROOT/sdk/VPINBALL_COMMIT"

echo "SDK headers updated to vpinball $COMMIT ($DATE)"
git -C "$ROOT" status --short -- sdk || true
