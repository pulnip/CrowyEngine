#!/bin/sh
# Captures StageEditor from every cut of Backlot's scene file in the chosen
# lighting keys, through the command port: a held loop, one property write
# per cut and key, one frame each. Names follow Backlot's own captures
# (<cut>.png for the default key, <cut>@<key>.png for the others).
#
# usage: Tools/stage_cuts.sh [keys, comma-separated] [out-dir] [exe] [backlot-dir] [port]
#   e.g. Tools/stage_cuts.sh day,night captures/stage/mac
#
# Debug builds only (the port). Run from the repository root. The editor
# listens on its own port (27520 unless given), so a sample already running
# is never driven.
set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO_ROOT/Tools/port.sh"
. "$REPO_ROOT/Tools/stage_editor.sh"

KEYS="${1:-}"
OUT="${2:-captures/stage/$(date +%Y%m%d-%H%M%S)}"
EXE="${3:-build/bin/StageEditor}"
BACKLOT="${4:-$REPO_ROOT/../Backlot}"
PORT="${5:-27520}"
TOOL="$(dirname "$EXE")/ImageCompareCheck"

SCENE="$BACKLOT/Data/scene.json"
CUTS=$(python3 -c 'import json, sys; print(" ".join(c["name"] for c in json.load(open(sys.argv[1], encoding="utf-8"))["cameras"]))' "$SCENE")
DEFAULT_KEY=$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["default_key"])' "$SCENE")
[ -n "$KEYS" ] || KEYS="$DEFAULT_KEY"

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
stage_editor_start "$EXE" "$PORT" || exit 1
# the editor's chrome stays out of the pictures
port_set debug showPanel false || exit 1

for KEY in $(echo "$KEYS" | tr ',' ' '); do
    set_editor key "$KEY" || exit 1
    for CUT in $CUTS; do
        set_editor cut "$CUT" || exit 1
        if [ "$KEY" = "$DEFAULT_KEY" ]; then NAME="$CUT"; else NAME="$CUT@$KEY"; fi
        # a key change rebuilds the walker on the next frame: capture the
        # one after it
        FRAME=$(port_frame) || exit 1
        stage_snap "$OUT/$NAME.bmp" "$TOOL" "$((FRAME + 2))" >/dev/null || exit 1
        echo "captured $NAME"
    done
done
echo "cuts: $OUT"
