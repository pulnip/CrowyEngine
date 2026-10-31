#!/bin/sh
# Playground's Scene View over the command port, as a person drives it with
# the mouse: a click on a sphere, its transform read, a meter east and a
# quarter turn on the gizmo, the old point missing it, the sun turned on its
# ring, and no press taken once the view is off. A fresh run then draws the
# launch picture again: nothing was written back.
# Every step is a property write the mouse makes too; the script exits 1 on
# the first that fails.
#
# usage: Tools/playground_edit.sh [out-dir] [exe] [port]
#
# Debug builds only (the port). Run from the repository root; python3 reads
# the JSON.
set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO_ROOT/Tools/port.sh"
cd "$REPO_ROOT" || exit 1

OUT="${1:-captures/playground-edit}"
EXE="${2:-build/bin/Debug/Playground}"
PORT="${3:-27540}"
TOOL="$(dirname "$EXE")/ImageCompareCheck"
if [ ! -x "$TOOL" ]; then
    echo "the run compares a capture with $TOOL; build ImageCompareCheck first" >&2
    exit 1
fi
case "$(uname -s)" in
Darwin) BACKEND=metal ;;
*) BACKEND=dx12 ;;
esac
BASELINE="Engine/Render/Sample/Golden/Playground.$BACKEND.png"

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
# dielectric-2-sphere's middle in the Scene View's image at launch, viewport
# points (measured)
SPHERE_POINT='[312, 183]'
SPHERE=dielectric-2-sphere

ok() {
    echo "ok: $1"
}

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

# json_is <python expression over v> < json: whether it holds
json_is() {
    python3 -c 'import json, sys; v = json.load(sys.stdin); sys.exit(0 if eval(sys.argv[1]) else 1)' "$1"
}

# json_at <python expression over v> < json: its value as JSON
json_at() {
    python3 -c 'import json, sys; v = json.load(sys.stdin); print(json.dumps(eval(sys.argv[1])))' "$1"
}

# step_frames [count]: that many frames let through and ended
step_frames() {
    STEP_TO=$(($(port_frame) + ${1:-1}))
    port_rpc run "{\"frames\":${1:-1}}" >/dev/null || exit 1
    port_rpc wait_frame "{\"frame\":$STEP_TO}" >/dev/null || exit 1
}

# drag_handle <gizmo aim> <handle name>: pressed at its grab, let go at its
# reach
drag_handle() {
    AIM=$(port_get gizmo "$1") || exit 1
    port_set editor grab "$(echo "$AIM" | json_at 'v["grab"]')"
    [ "$(port_get editor handle)" = "\"$2\"" ] || fail "a press at the $1 aim holds $2 ($(port_get editor status))"
    port_set editor drag "$(echo "$AIM" | json_at 'v["reach"]')"
    port_set editor handle None
}

# playground_stop: quit asked, 30 s given, the exit status returned
playground_stop() {
    port_rpc quit '{}' >/dev/null 2>&1
    port_rpc run '{}' >/dev/null 2>&1
    WAITED=0
    while kill -0 "$PLAYGROUND_PID" 2>/dev/null; do
        if [ "$WAITED" -ge 300 ]; then
            kill "$PLAYGROUND_PID" 2>/dev/null
            wait "$PLAYGROUND_PID" 2>/dev/null
            return 1
        fi
        WAITED=$((WAITED + 1))
        sleep 0.1
    done
    wait "$PLAYGROUND_PID" 2>/dev/null
}

port_use "$PORT"
if port_rpc ping '{}' >/dev/null 2>&1; then
    echo "port $PORT already answers; close what holds it first" >&2
    exit 1
fi
(
    CROWY_DEBUG='{"sceneView":true}'
    export CROWY_DEBUG
    CROWY_WINDOW_DISPLAY=1 CROWY_COMMAND_PORT="$PORT" exec "$EXE" --hold
) >/dev/null 2>&1 &
PLAYGROUND_PID=$!
trap 'playground_stop' EXIT
port_wait 120 || exit 1
[ "$(port_rpc ping '{}' | port_field app)" = '"Playground"' ] || fail "port $PORT answers for Playground"
step_frames 3

# 1. a click on the sphere
port_set editor pickAt "$SPHERE_POINT"
SELECTED=$(port_get editor selected)
[ "$SELECTED" = "\"$SPHERE\"" ] || fail "a click at $SPHERE_POINT selects $SPHERE (got $SELECTED)"
ok "a click at $SPHERE_POINT selects $SPHERE"

# 2. its transform, a meter east on the X arrow, then a quarter turn on the
#    Y ring, snapped
POSITION=$(port_get selection position)
port_set editor snap true
drag_handle moveX MoveX
printf '{"was": %s, "now": %s}' "$POSITION" "$(port_get selection position)" |
    json_is 'v["now"][0] == v["was"][0] + 1 and v["now"][1:] == v["was"][1:]' || fail "the arrow moved the sphere 1 m east"
ok "the arrow moved the sphere 1 m east"
drag_handle ring Ring
port_get selection rotation |
    json_is 'all(abs(a - b) < 1e-5 for a, b in zip(v, [0, 0.70710678, 0, 0.70710678]))' || fail "the ring turned it 90 degrees about Y ($(port_get selection rotation))"
ok "the ring turned it 90 degrees about Y"
port_set editor snap false

# 3. the rows follow at the next frame: the old point misses the sphere
step_frames 2
port_set editor pickAt "$SPHERE_POINT"
[ "$(port_get editor selected)" != "\"$SPHERE\"" ] || fail "a click at the old point no longer selects the sphere"
ok "a click at the old point misses the moved sphere"

# 4. the sun on its ring: the selection and Playground's own light target
#    read one rotation, a quarter turn about world Y onto the file's
port_set editor selected sun
SUN_BEFORE=$(port_get light.sun.transform rotation)
port_set editor snap true
drag_handle ring Ring
port_set editor snap false
printf '{"was": %s, "now": %s, "selection": %s}' "$SUN_BEFORE" "$(port_get light.sun.transform rotation)" "$(port_get selection rotation)" |
    json_is '
(lambda q, r: v["now"] == v["selection"] and all(abs(a - b) < 1e-5 for a, b in zip(v["now"], [
    q[3] * r[0] + q[0] * r[3] + q[1] * r[2] - q[2] * r[1],
    q[3] * r[1] - q[0] * r[2] + q[1] * r[3] + q[2] * r[0],
    q[3] * r[2] + q[0] * r[1] - q[1] * r[0] + q[2] * r[3],
    q[3] * r[3] - q[0] * r[0] - q[1] * r[1] - q[2] * r[2]])))([0, 0.70710678, 0, 0.70710678], v["was"])' ||
    fail "the ring turned the sun a quarter about world Y"
ok "the ring turned the sun a quarter about world Y, as light.sun.transform reads it"

# 5. nothing selected, then no view to press on
port_set editor selected '""'
[ "$(port_get editor selected)" = '""' ] || fail "an empty name clears the selection"
ok "an empty name clears the selection"
port_set debug sceneView false
step_frames 2
port_set editor pickAt "$SPHERE_POINT"
[ "$(port_get editor selected)" = '""' ] || fail "a click with the view off selects nothing"
ok "a click with the view off selects nothing"
trap - EXIT
playground_stop || fail "Playground quit by itself and exited 0"

# 6. nothing was written back: a fresh run draws the baseline
(
    unset CROWY_DEBUG
    CROWY_COMMAND_PORT=0 CROWY_WINDOW_DISPLAY=1 Tools/smoke_run.sh "$EXE" "$OUT/fresh.bmp" >/dev/null 2>&1
) || fail "the fresh run captured frame 60"
"$TOOL" "$OUT/fresh.png" "$BASELINE" --tolerance 0 >/dev/null || fail "the fresh run draws $BASELINE"
ok "a fresh run draws $BASELINE at tolerance 0"

echo "the edit ran: $OUT"
