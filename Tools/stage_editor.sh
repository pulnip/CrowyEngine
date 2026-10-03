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
    if pgrep -x StageEditor >/dev/null 2>&1; then
        echo "a StageEditor is already running; close it first" >&2
        return 1
    fi
    port_use "$2"
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
