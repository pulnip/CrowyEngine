#!/bin/sh
# The owner's one task in StageEditor, performed over the command port as
# Tools/stage_task.ps1 performs it: the street cut, a click on the street
# lamp, its row read, a meter east and a quarter turn on the gizmo, night,
# reloads of the scene file (the same file, a scratch copy, a refused copy,
# the original), and scene time. Every step is a property write the panels
# and the mouse make too; the script exits 1 on the first that fails.
#
# usage: Tools/stage_task.sh [out-dir] [exe] [backlot-dir] [port]
#
# Debug builds only (the port). Run from the repository root; python3 reads
# the JSON.
set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO_ROOT/Tools/port.sh"
. "$REPO_ROOT/Tools/stage_editor.sh"

OUT="${1:-captures/stage-task}"
EXE="${2:-build/bin/StageEditor}"
BACKLOT="${3:-$REPO_ROOT/../Backlot}"
PORT="${4:-27520}"
TOOL="$(dirname "$EXE")/ImageCompareCheck"
if [ ! -x "$TOOL" ]; then
    echo "the task compares captures with $TOOL; build ImageCompareCheck first" >&2
    exit 1
fi

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
SCENE_FILE="$(cd "$BACKLOT/Data" && pwd)/scene.json"
# lamp-ne's pole in the street cut at 1920 x 1080 (measured against Backlot a6179ef)
LAMP_PIXEL='[1076, 450]'
LAMP=instance/lamp-ne

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

# a write lands on the next frame: capture the one after it
snap() {
    FRAME=$(port_frame) || fail "the port stopped answering"
    stage_snap "$OUT/$1.bmp" "$TOOL" "$((FRAME + 2))" || fail "captured $1"
}

same() {
    "$TOOL" "$1" "$2" --tolerance 0 >/dev/null
}

# the scene file's lamp-ne row, as the JSON the port gives: [x, y, z], yaw, [sx, sy, sz]
LAMP_ROW=$(python3 -c '
import json, sys
row = next(r for r in json.load(open(sys.argv[1], encoding="utf-8"))["instances"] if r["name"] == "lamp-ne")
print(json.dumps({"position": [row["x"], row["y"], row["z"]], "yaw": row["yaw"], "scale": [row["sx"], row["sy"], row["sz"]]}))
' "$SCENE_FILE")

stage_editor_start "$EXE" "$PORT" || exit 1
port_set debug showPanel false || fail "the chrome hidden"

# 0. the launch: the first revision of Backlot's own scene file
ORIGINAL=$(port_get editor scene) || exit 1
port_get editor revision | json_is 'v == 1' || fail "revision 1 at launch"
echo "$ORIGINAL" | json_is 'v.replace(chr(92), "/").endswith("Data/scene.json")' || fail "the scene file is Backlot's ($ORIGINAL)"
port_get editor paused | json_is 'v is True' && port_get editor time | json_is 'v == 0' || fail "scene time is paused at 0"
ok "the launch reads Backlot's scene file, revision 1, paused at 0"

# 1. the street cut
set_editor cut street || fail "the street cut"
port_rpc run '{"frames": 3}' >/dev/null
ok "the street cut"

# 1b. reading the same file again redraws the same picture
set_editor key night || fail "the night key"
NIGHT_BEFORE=$(snap night-before) || exit 1
port_set editor reload true
port_get editor reload | json_is 'v is True' || fail "a reload waits for the next frame"
NIGHT_AGAIN=$(snap night-reloaded-same) || exit 1
port_get editor revision | json_is 'v == 2' || fail "the reload read the file: $(port_get editor status)"
same "$NIGHT_BEFORE" "$NIGHT_AGAIN" || fail "a reload of the same file redraws the same picture"
ok "a reload of the same file redraws the same picture at tolerance 0"
set_editor key day || fail "the day key"

# 2. a click on the lamp's pole
port_set editor pickAt "$LAMP_PIXEL"
SELECTED=$(port_get editor selected)
[ "$SELECTED" = "\"$LAMP\"" ] || fail "a click at $LAMP_PIXEL selects $LAMP (got $SELECTED)"
ok "a click at $LAMP_PIXEL selects $LAMP"

# 3. its transform, as the scene file has them
POSITION=$(port_get selection position)
printf '{"read": {"position": %s, "yaw": %s, "scale": %s}, "file": %s}' "$POSITION" "$(port_get selection yaw)" "$(port_get selection scale)" "$LAMP_ROW" |
    json_is 'all(abs(a - b) < 1e-4 for a, b in zip(v["read"]["position"] + v["read"]["scale"] + [v["read"]["yaw"]], v["file"]["position"] + v["file"]["scale"] + [v["file"]["yaw"]]))' ||
    fail "its position, yaw and scale are the scene file's"
ok "its position, yaw and scale are the scene file's"

# 4. with the gizmo, pressed and dragged where a person sees its handles: a
#    meter east on the X arrow, then a quarter turn on the ring, snapped
BEFORE=$(snap before-move) || exit 1
port_set debug showPanel true
FRAME=$(port_frame) || exit 1
port_rpc run '{"frames": 1}' >/dev/null
port_rpc wait_frame "{\"frame\": $((FRAME + 1))}" >/dev/null
port_set editor snap true
AIM=$(port_get gizmo moveX)
port_set editor grab "$(echo "$AIM" | json_at 'v["grab"]')"
[ "$(port_get editor handle)" = '"MoveX"' ] || fail "a press at the X arrow's tip holds it"
port_set editor drag "$(echo "$AIM" | json_at 'v["reach"]')"
port_set editor handle None
printf '{"was": %s, "now": %s}' "$POSITION" "$(port_get selection position)" |
    json_is 'v["now"][0] == v["was"][0] + 1 and v["now"][1:] == v["was"][1:]' || fail "the arrow moved the lamp 1 m east"
ok "the arrow moved the lamp 1 m east"
RING=$(port_get gizmo ring)
port_set editor grab "$(echo "$RING" | json_at 'v["grab"]')"
[ "$(port_get editor handle)" = '"Ring"' ] || fail "a press on the ring holds it"
port_set editor drag "$(echo "$RING" | json_at 'v["reach"]')"
port_set editor handle None
port_get selection yaw | json_is 'v == 270' || fail "the ring turned it 90 degrees to yaw 270"
ok "the ring turned it 90 degrees to yaw 270"
port_set editor snap false
port_set debug showPanel false
AFTER=$(snap after-move) || exit 1
same "$BEFORE" "$AFTER" && fail "the capture after the move differs from the one before"
ok "the capture after the move differs from the one before"

# the pick follows the move: the old pixel misses the lamp
port_set editor pickAt "$LAMP_PIXEL"
[ "$(port_get editor selected)" != "\"$LAMP\"" ] || fail "a click at the old pixel no longer selects the lamp"
port_set editor pickAt '[1100, 450]'
[ "$(port_get editor selected)" = "\"$LAMP\"" ] || fail "a click at 1100, 450 selects the moved lamp"
ok "the pick follows the lamp from x 1076 to 1100"

# 5. night, and a capture
set_editor key night || fail "the night key"
NIGHT=$(snap night) || exit 1
ok "captured $NIGHT"

# 6. a scratch copy of the scene file with the lamp where the gizmo put it:
#    reloaded, it draws the gizmo's picture exactly
LAMP_LINE='"name": "lamp-ne", "area": "Street", "model": "StreetLamp", "x": 10.5, "y": 0.15, "z": 7.0, "yaw": 180.0,'
MOVED_LINE='"name": "lamp-ne", "area": "Street", "model": "StreetLamp", "x": 11.5, "y": 0.15, "z": 7.0, "yaw": 270.0,'
python3 -c '
import sys
text = open(sys.argv[1], encoding="utf-8", newline="").read()
if text.count(sys.argv[2]) != 1:
    sys.exit(1)
moved = text.replace(sys.argv[2], sys.argv[3])
open(sys.argv[4], "w", encoding="utf-8", newline="").write(moved)
refused = moved.replace("Models/Street/StreetLamp.fbx", "Models/Street/StreetLampMoved.fbx")
open(sys.argv[5], "w", encoding="utf-8", newline="").write(refused)
' "$SCENE_FILE" "$LAMP_LINE" "$MOVED_LINE" "$OUT/scene-moved.json" "$OUT/scene-refused.json" || fail "the scene file holds lamp-ne's row once"
MOVED_FILE=$(stage_native_path "$OUT/scene-moved.json")
port_set editor scene "$(port_quote "$MOVED_FILE")"
port_set editor reload true
RELOADED=$(snap night-reloaded-moved) || exit 1
port_get editor revision | json_is 'v == 3' || fail "the scratch copy loaded: $(port_get editor status)"
[ "$(port_get editor selected)" = "\"$LAMP\"" ] || fail "the selection is kept by name"
port_get selection position | json_is 'v[0] == 11.5' && port_get selection yaw | json_is 'v == 270' || fail "the file puts the lamp at x 11.5, yaw 270"
same "$NIGHT" "$RELOADED" || fail "the reloaded copy draws the gizmo's picture"
ok "the scratch copy reloads to the gizmo's picture at tolerance 0"

# 7. a file naming geometry the launch did not load is refused whole
port_set editor scene "$(port_quote "$(stage_native_path "$OUT/scene-refused.json")")"
port_set editor reload true
REFUSED=$(snap night-refused) || exit 1
port_get editor revision | json_is 'v == 3' || fail "the refused reload keeps revision 3"
port_get editor status | json_is '"restart" in v' || fail "the status says a restart loads it: $(port_get editor status)"
[ "$(port_get editor scene)" = "$(port_quote "$MOVED_FILE")" ] || fail "the scene file goes back to the one the rows came from"
same "$RELOADED" "$REFUSED" || fail "a refused reload changes nothing on screen"
ok "a file naming geometry the launch did not load is refused, nothing changes"

# 8. Backlot's own file again: the file wins over the gizmo
port_set editor scene "$ORIGINAL"
port_set editor reload true
RESTORED=$(snap night-restored) || exit 1
port_get editor revision | json_is 'v == 4' || fail "the original file loaded again"
port_get selection position | json_is 'v[0] == 10.5' && port_get selection yaw | json_is 'v == 180' || fail "the lamp is back at x 10.5, yaw 180"
same "$NIGHT_BEFORE" "$RESTORED" || fail "the original file redraws the launch's picture"
ok "Backlot's file restores the launch's picture at tolerance 0"

# 9. scene time: a seek shows its frame, a counted run plays the loop's
#    step, and the same play twice draws the same picture
port_set editor selected quad/screen-ne
port_set editor time 0.32
port_get selection.material uvScaleOffset | json_is 'v == [0.5, 0.25, 0, 0.25]' || fail "at 0.32 s screen-ne shows frame 2"
port_set selection.material uvScaleOffset '[1, 1, 0, 0]'
port_get selection.material uvScaleOffset | json_is 'v == [0.5, 0.25, 0, 0.25]' || fail "a write to the rect is drawn over by the row and the clock"
FRAME=$(port_frame)
port_set editor paused false
port_rpc run '{"frames": 30}' >/dev/null
port_rpc wait_frame "{\"frame\": $((FRAME + 30))}" >/dev/null
port_set editor paused true
port_get editor time | json_is 'abs(v - 0.82) < 1e-4' || fail "30 counted frames play to 0.82 s"
port_get selection.material uvScaleOffset | json_is 'v == [0.5, 0.25, 0.5, 0.5]' || fail "0.82 s shows frame 5"
ok "a seek shows its frame, 30 counted frames play to 0.82 s"
for TAKE in 1 2; do
    port_set editor time 0.32
    port_set editor paused false
    FRAME=$(port_frame)
    stage_snap "$OUT/play-$TAKE.bmp" "$TOOL" "$((FRAME + 30))" >/dev/null || fail "captured play $TAKE"
    port_set editor paused true
done
same "$OUT/play-1.png" "$OUT/play-2.png" || fail "the same play twice draws the same picture"
port_set editor time 0
STILL=$(snap time-zero) || exit 1
same "$STILL" "$OUT/play-1.png" && fail "the played picture shows another frame than time 0"
port_get selection.material uvScaleOffset | json_is 'v == [0.5, 0.25, 0, 0]' || fail "time 0 shows the first frame again"
ok "the same play twice draws the same picture at tolerance 0"

echo "the task ran: $OUT"
