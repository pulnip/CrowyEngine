#!/bin/sh
# StageEditor driven over its command port, shared by stage_cuts.sh and
# stage_task.sh; source it after port.sh.

# a path the editor can open: Git Bash's /c/... becomes C:\...
stage_native_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s\n' "$1"
    fi
}

# whether the port answers for StageEditor
stage_editor_answers() {
    [ "$(port_rpc ping '{}' 2>/dev/null | port_field app 2>/dev/null)" = '"StageEditor"' ]
}

# quits the editor over the port when it is the one answering, then ends it
stage_editor_stop() {
    if stage_editor_answers; then
        port_rpc quit '{}' >/dev/null 2>&1
        sleep 2
    fi
    kill "$STAGE_EDITOR_PID" 2>/dev/null
}

# stage_editor_start <exe> <port>: launches the editor held, on <port> alone,
# and waits until it answers there; the EXIT trap quits it
stage_editor_start() {
    port_use "$2"
    # Git Bash has no pgrep: whatever already answers on the port is refused
    if port_rpc ping '{}' >/dev/null 2>&1; then
        echo "port $2 already answers; close what holds it first" >&2
        return 1
    fi
    # the child alone takes the port, and without retries
    CROWY_COMMAND_PORT="$2" "$1" --hold >/dev/null 2>&1 &
    STAGE_EDITOR_PID=$!
    trap 'stage_editor_stop' EXIT
    port_wait 120 || return 1
    if ! stage_editor_answers; then
        echo "port $2 does not answer for StageEditor" >&2
        return 1
    fi
}

# set_editor <field> <value>: the editor reverts a write it cannot apply;
# read it back
set_editor() {
    port_set editor "$1" "$2" || return 1
    NOW=$(port_get editor "$1")
    if [ "$NOW" != "$(port_quote "$2")" ]; then
        echo "editor.$1 stayed $NOW: $(port_get editor status)" >&2
        return 1
    fi
}

# stage_snap <bmp> <tool> [frame]: captures to <bmp> (absolute), converted to
# a PNG beside it when the tool is there; prints the file that stays
stage_snap() {
    port_snap "$(stage_native_path "$1")" "${3:-}" || return 1
    if [ ! -f "$1" ]; then
        echo "the capture $1 was not written" >&2
        return 1
    fi
    if [ -x "$2" ]; then
        "$2" --convert "$1" "${1%.bmp}.png" >/dev/null || return 1
        rm -f "$1"
        echo "${1%.bmp}.png"
    else
        echo "$1"
    fi
}

# a JSON string on stdin, unquoted
stage_unquote() {
    python3 -c 'import json, sys; print(json.load(sys.stdin))'
}

# the content root of the scene file the editor launched with:
# <root>/Data/scene.json, as a path this shell can read
stage_root() {
    ROOT_FILE="$1"
    if command -v cygpath >/dev/null 2>&1; then
        ROOT_FILE=$(cygpath -u "$ROOT_FILE")
    fi
    dirname "$(dirname "$ROOT_FILE")"
}

# backlot_state <root>: BACKLOT_HEAD, empty when git cannot read the root,
# and BACKLOT_CHANGES, its tracked changes under what the editor reads
backlot_state() {
    BACKLOT_HEAD=$(git -C "$1" rev-parse HEAD 2>/dev/null) || BACKLOT_HEAD=""
    BACKLOT_CHANGES=""
    if [ -n "$BACKLOT_HEAD" ]; then
        BACKLOT_CHANGES=$(git -C "$1" status --porcelain --untracked-files=no -- Data Unity/Assets/Art 2>/dev/null)
    fi
}

# stage_stamp <file>: STAMP_BACKLOT and STAMP_KEYS (space-separated), the
# Backlot commit StageEditor's goldens were taken at and the keys they pin;
# nonzero without a stamp
stage_stamp() {
    STAMP_BACKLOT=""
    STAMP_KEYS=""
    [ -f "$1" ] || return 1
    STAMP_BACKLOT=$(sed -n 's/^backlot \([^ ]*\).*/\1/p' "$1" | head -n 1)
    STAMP_KEYS=$(sed -n 's/^keys //p' "$1" | head -n 1)
}

# stage_stamp_write <file> <backlot> <keys>
stage_stamp_write() {
    {
        echo "# StageEditor's goldens: the Backlot commit they were taken at and the"
        echo "# lighting keys pinned for every cut; the launch picture is StageEditor.<backend>.png"
        echo "backlot $2"
        echo "keys $3"
    } >"$1"
}
