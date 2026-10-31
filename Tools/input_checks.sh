#!/bin/sh
# The input layer checked through the command port, with no hand on the
# mouse: Playground runs held with --port-input, so its devices are not heard
# and its window never takes the focus, and every frame is a counted step.
# The script pushes the events a person would make and asserts what the
# camera and read_input report; it exits 1 on the first check that fails.
#
# usage: Tools/input_checks.sh [--exe build/bin/Debug/Playground] [--port 27530]
#
# Debug builds only (the port). Run from the repository root. JSON is read
# with python3.
set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO_ROOT/Tools/port.sh"

usage() {
    sed -n 's/^# \{0,1\}//; /^usage:/,/^$/p' "$0" | sed '/^$/d' >&2
    exit 2
}

EXE="build/bin/Debug/Playground"
PORT=27530
while [ $# -gt 0 ]; do
    case "$1" in
    --exe | --port)
        [ $# -ge 2 ] || usage
        case "$1" in
        --exe) EXE="$2" ;;
        --port) PORT="$2" ;;
        esac
        shift 2
        ;;
    *)
        usage
        ;;
    esac
done
cd "$REPO_ROOT" || exit 1

# Playground's fly camera: radians per mouse point, meters per second; a
# step is one fixed tick of 1/60 s
LOOK=0.003
# what a look and a walk move, the game camera until the Scene View runs
LOOK_TARGET=camera.look
WALK_TARGET=camera.transform
SPEED=5.0

# window points in a 1280 x 720 run with no saved layout: the scene, the
# panel P opens at 8,8 360 x 640, its title bar, and the Scene View image
# (window 376,8 640 x 400)
SCENE="900, 500"
PANEL="150, 300"
TITLE="120, 18"
IMAGE="696, 220"

# check <python expression> <what>: ends the script when it is false
check() {
    if python3 -c "import sys; sys.exit(0 if ($1) else 1)"; then
        echo "ok: $2"
    else
        echo "FAIL: $2" >&2
        exit 1
    fi
}

# json_value <json> <python expression of j>
json_value() {
    printf '%s' "$1" | python3 -c 'import json, sys; j = json.load(sys.stdin); print(json.dumps(eval(sys.argv[1])))' "$2"
}

# step_frames [count]: that many frames let through and ended
step_frames() {
    STEP_TO=$(( $(port_frame) + ${1:-1} ))
    port_rpc run "{\"frames\":${1:-1}}" >/dev/null || exit 1
    port_rpc wait_frame "{\"frame\":$STEP_TO}" >/dev/null || exit 1
}

# push_input <JSON array of events>: they go in at the next step
push_input() {
    port_rpc push_input "{\"events\":$1}" >/dev/null || exit 1
}

# move_mouse "<x>, <y>": the UI sees what is under it in the frame after,
# and the gate it sets reaches the app in the one after that
move_mouse() {
    push_input "[{\"moveTo\":[$1]}]"
    step_frames 3
}

# click_key <key>: a press and a release in frames of their own, as ImGui
# takes one change of a button per frame
click_key() {
    push_input "[{\"key\":\"$1\",\"down\":true}]"
    step_frames 1
    push_input "[{\"key\":\"$1\",\"down\":false}]"
    step_frames 1
}

press() {
    push_input "[{\"button\":\"$1\",\"down\":$2}]"
}

read_yaw() {
    port_get "$LOOK_TARGET" yaw
}

read_input() {
    port_rpc read_input '{}' || exit 1
}

# drag_look <dx>: a right-button drag, each part its own frame; DURING holds
# read_input from the frame that turned
drag_look() {
    press RButton true
    step_frames 1
    push_input "[{\"move\":[$1, 0]}]"
    step_frames 1
    DURING=$(read_input)
    press RButton false
    step_frames 1
}

# walk [frames]: WALKED is the meters walked with W held that many frames
walk() {
    BEFORE=$(port_get "$WALK_TARGET" position)
    push_input '[{"key":"W","down":true}]'
    step_frames "${1:-12}"
    push_input '[{"key":"W","down":false}]'
    step_frames 1
    AFTER=$(port_get "$WALK_TARGET" position)
    WALKED=$(python3 -c 'import json, math, sys; a, b = json.loads(sys.argv[1]), json.loads(sys.argv[2]); print(math.dist(a, b))' "$BEFORE" "$AFTER")
}

playground_stop() {
    port_rpc quit '{}' >/dev/null 2>&1
    port_rpc run '{}' >/dev/null 2>&1
    wait "$PLAYGROUND_PID" 2>/dev/null
}

# playground_start [debug]: held, on PORT alone, hearing only the port
playground_start() {
    port_use "$PORT"
    if port_rpc ping '{}' >/dev/null 2>&1; then
        echo "port $PORT already answers; close what holds it first" >&2
        exit 1
    fi
    (
        if [ -n "${1:-}" ]; then
            CROWY_DEBUG="$1"
            export CROWY_DEBUG
        else
            unset CROWY_DEBUG
        fi
        CROWY_COMMAND_PORT="$PORT" exec "$EXE" --hold --port-input
    ) >/dev/null 2>&1 &
    PLAYGROUND_PID=$!
    trap 'playground_stop' EXIT
    port_wait 120 || exit 1
    if [ "$(port_rpc ping '{}' | port_field app)" != '"Playground"' ]; then
        echo "port $PORT does not answer for Playground" >&2
        exit 1
    fi
    step_frames 2
}

playground_start ""

# 1. a look on the scene turns by the motion, with the cursor hidden
move_mouse "$SCENE"
YAW=$(read_yaw)
drag_look 600
AFTER_LOOK=$(read_input)
check "abs($(read_yaw) - $YAW - 600 * $LOOK) <= 1e-4" "a right drag of 600 points turns the camera 1.8 rad"
check "'$(json_value "$DURING" 'j["relativeMouse"]')' == 'true'" "the look hides the cursor while it turns"
check "'$(json_value "$AFTER_LOOK" 'j["relativeMouse"]')' == 'false'" "letting go shows it again"
check "$(json_value "$AFTER_LOOK" 'j["mouse"]["pos"]') == [$SCENE]" "the cursor is back where the look began"

# 2. the panel keeps the mouse it is under
click_key P
move_mouse "$PANEL"
check "'$(json_value "$(read_input)" 'j["ui"]["mouse"]')' == 'true'" "the panel under the mouse has it"
YAW=$(read_yaw)
drag_look 300
check "abs($(read_yaw) - $YAW) <= 1e-4" "a right drag on the panel leaves the camera"

move_mouse "$PANEL"
YAW=$(read_yaw)
press RButton true
step_frames 1
push_input '[{"move":[600, 0]}]'
step_frames 1
push_input '[{"move":[50, 0]}]'
step_frames 1
press RButton false
step_frames 1
check "abs($(read_yaw) - $YAW) <= 1e-4" "a drag pressed on the panel stays the panel's off it"

move_mouse "$SCENE"
YAW=$(read_yaw)
drag_look -700
check "abs($(read_yaw) - $YAW + 700 * $LOOK) <= 1e-4" "a drag pressed on the scene keeps turning over the panel"

# 3. the wheel scales the walk, unless the panel is under it
move_mouse "$SCENE"
walk
BASE=$WALKED
check "abs($BASE - 12 / 60 * $SPEED) <= 1e-3" "W for 12 frames walks 1 m"
push_input '[{"wheel":4}]'
step_frames 1
walk
FAST=$WALKED
check "abs($FAST / $BASE - 1.25 ** 4) <= 1e-3" "four notches walk 1.25^4 as far"
move_mouse "$PANEL"
push_input '[{"wheel":-4}]'
step_frames 1
walk
check "abs($WALKED - $FAST) <= 1e-3" "the wheel on the panel leaves the speed"
move_mouse "$SCENE"
push_input '[{"wheel":-4}]'
step_frames 1

# 4. a held key reads up while the UI holds an item
move_mouse "$TITLE"
press LButton true
step_frames 3
check "'$(json_value "$(read_input)" 'j["ui"]["keyboard"]')' == 'true'" "holding the panel's title bar takes the keyboard"
walk
check "abs($WALKED) <= 1e-6" "W walks nowhere while it does"
press LButton false
step_frames 1
click_key P

playground_stop
trap - EXIT

# 5. the Scene View's image gives its presses to the scene, whose camera is
#    the editor's: the game camera stays where it stands
LOOK_TARGET=sceneCamera
WALK_TARGET=sceneCamera
playground_start '{"sceneView":true}'
GAME_CAMERA=$(port_get camera.transform position)
move_mouse "$IMAGE"
check "'$(json_value "$(read_input)" 'j["ui"]["mouse"]')' == 'false'" "the image under the mouse leaves it to the scene"
YAW=$(read_yaw)
drag_look 400
check "abs($(read_yaw) - $YAW - 400 * $LOOK) <= 1e-4" "a right drag on the image turns the camera"
press LButton true
step_frames 3
check "'$(json_value "$(read_input)" 'j["ui"]["keyboard"]')' == 'false'" "a press held on the image leaves the keyboard to the scene"
walk
check "$WALKED > 0.5" "W walks while it does"
check "'$(port_get camera.transform position)' == '$GAME_CAMERA'" "the game camera stays where it stands"
press LButton false
step_frames 1
YAW=$(read_yaw)
drag_look 100
check "abs($(read_yaw) - $YAW - 100 * $LOOK) <= 1e-4" "the image is still under the same point after the left drag"
click_key P
move_mouse "$PANEL"
YAW=$(read_yaw)
drag_look 300
check "abs($(read_yaw) - $YAW) <= 1e-4" "a right drag on the panel beside it leaves the camera"

echo "the input checks ran"
