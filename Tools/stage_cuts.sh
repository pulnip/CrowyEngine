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

KEYS="${1:-}"
OUT="${2:-captures/stage/$(date +%Y%m%d-%H%M%S)}"
EXE="${3:-build/bin/StageEditor}"
BACKLOT="${4:-$REPO_ROOT/../Backlot}"
PORT="${5:-27520}"
TOOL="$(dirname "$EXE")/ImageCompareCheck"
port_use "$PORT"

SCENE="$BACKLOT/Data/scene.json"
CUTS=$(python3 -c 'import json, sys; print(" ".join(c["name"] for c in json.load(open(sys.argv[1]))["cameras"]))' "$SCENE")
DEFAULT_KEY=$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["default_key"])' "$SCENE")
[ -n "$KEYS" ] || KEYS="$DEFAULT_KEY"

# the editor reverts a write it cannot apply; read it back
set_editor() {
    port_set editor "$1" "$2" || return 1
    NOW=$(port_get editor "$1")
    if [ "$NOW" != "$(port_quote "$2")" ]; then
        echo "editor.$1 stayed $NOW: $(port_get editor status)" >&2
        return 1
    fi
}

# whether the port answers for the editor this script launched
is_editor() {
    [ "$(port_rpc ping '{}' 2>/dev/null | port_field app 2>/dev/null)" = '"StageEditor"' ]
}

if pgrep -x StageEditor >/dev/null 2>&1; then
    echo "a StageEditor is already running; close it first" >&2
    exit 1
fi
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
# the child alone takes the port, and without retries
CROWY_COMMAND_PORT="$PORT" "$EXE" --hold >/dev/null 2>&1 &
PID=$!
trap 'if is_editor; then port_rpc quit "{}" >/dev/null 2>&1; sleep 2; fi; kill "$PID" 2>/dev/null' EXIT

port_wait 120 || exit 1
if ! is_editor; then
    echo "port $PORT does not answer for StageEditor" >&2
    exit 1
fi
# the editor's chrome stays out of the pictures
port_set debug showPanel false || exit 1

for KEY in $(echo "$KEYS" | tr ',' ' '); do
    set_editor key "$KEY" || exit 1
    for CUT in $CUTS; do
        set_editor cut "$CUT" || exit 1
        if [ "$KEY" = "$DEFAULT_KEY" ]; then NAME="$CUT"; else NAME="$CUT@$KEY"; fi
        BMP="$OUT/$NAME.bmp"
        # a key change rebuilds the walker on the next frame: capture the
        # one after it
        FRAME=$(port_frame) || exit 1
        port_snap "$BMP" "$((FRAME + 2))" || exit 1
        if [ ! -f "$BMP" ]; then
            echo "the capture $BMP was not written" >&2
            exit 1
        fi
        if [ -x "$TOOL" ]; then
            "$TOOL" --convert "$BMP" "${BMP%.bmp}.png" >/dev/null || exit 1
            rm -f "$BMP"
        fi
        echo "captured $NAME"
    done
done
echo "cuts: $OUT"
