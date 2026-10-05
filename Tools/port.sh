#!/bin/sh
# The command port from a POSIX shell: source it, then call the functions.
# Debug builds only; the port listens on 127.0.0.1:27500
# (CROWY_COMMAND_PORT overrides, or port_use after sourcing). JSON is read
# with python3.
#
#   . Tools/port.sh
#   port_wait
#   port_set editor cut street
#   port_get camera position
#   port_snap captures/street.bmp
#   port_rpc quit '{}'
#
# Every function returns nonzero when the port answers with an error or
# stops answering.

# port_use <number>
port_use() {
    PORT_URI="http://127.0.0.1:$1/rpc"
}

port_use "${CROWY_COMMAND_PORT:-27500}"

# a JSON string from a shell string
port_quote() {
    python3 -c 'import json, sys; print(json.dumps(sys.argv[1]))' "$1"
}

# port_field <name>: the named field of the JSON on stdin, as JSON
port_field() {
    python3 -c 'import json, sys; print(json.dumps(json.load(sys.stdin)[sys.argv[1]]))' "$1"
}

# one verb with JSON arguments; prints its result as JSON
port_rpc() {
    ANSWER=$(curl -s -m 30 -X POST "$PORT_URI" -d "{\"cmd\":\"$1\",\"args\":${2:-{\}}}") || return 1
    printf '%s' "$ANSWER" | python3 -c '
import json, sys
answer = json.load(sys.stdin)
if not answer.get("ok"):
    sys.stderr.write("port: " + sys.argv[1] + " failed: " + str(answer.get("error")) + "\n")
    sys.exit(1)
print(json.dumps(answer.get("result")))
' "$1"
}

# polls ping until the port answers or the timeout runs out
port_wait() {
    WAITED=0
    while [ "$WAITED" -lt "${1:-60}" ]; do
        if port_rpc ping '{}' >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
        WAITED=$((WAITED + 1))
    done
    echo "port: no answer on $PORT_URI" >&2
    return 1
}

# port_set <target> <path> <value>: a value that is not JSON is sent as a
# string
port_set() {
    VALUE="$3"
    if ! printf '%s' "$VALUE" | python3 -c 'import json, sys; json.loads(sys.stdin.read())' 2>/dev/null; then
        VALUE=$(port_quote "$VALUE")
    fi
    port_rpc set_property "{\"target\":\"$1\",\"path\":\"$2\",\"value\":$VALUE}" >/dev/null
}

# port_get <target> [path]: the value as JSON, a leaf or a whole struct
port_get() {
    if [ -n "${2:-}" ]; then
        GET_ARGS="{\"target\":\"$1\",\"path\":\"$2\"}"
    else
        GET_ARGS="{\"target\":\"$1\"}"
    fi
    port_rpc get_property "$GET_ARGS" | port_field value
}

# the frame the loop last ended
port_frame() {
    port_rpc ping '{}' | port_field frame
}

# port_snap <path> [frame]: captures `frame`, or the next frame the loop
# starts, to an absolute path; runs a held loop up to it and returns once the
# file is written, nonzero when the write failed
port_snap() {
    STATUS=$(port_rpc ping '{}') || return 1
    FAILURES=$(printf '%s' "$STATUS" | port_field captureFailures) || return 1
    HELD=$(printf '%s' "$STATUS" | port_field held) || return 1
    if [ -n "${2:-}" ]; then
        SNAP_ARGS="{\"path\":$(port_quote "$1"),\"frame\":$2}"
    else
        SNAP_ARGS="{\"path\":$(port_quote "$1")}"
    fi
    FRAME=$(port_rpc capture_frame "$SNAP_ARGS" | python3 -c 'import json, sys; print(json.load(sys.stdin)["frames"][0])') || return 1
    if [ "$HELD" = true ]; then
        port_rpc run "{\"until\":$FRAME}" >/dev/null || return 1
    fi
    port_rpc wait_frame "{\"frame\":$FRAME}" >/dev/null || return 1

    POLLS=0
    while :; do
        STATUS=$(port_rpc ping '{}') || return 1
        PENDING=$(printf '%s' "$STATUS" | port_field capturesPending) || return 1
        [ "$PENDING" = 0 ] && break
        POLLS=$((POLLS + 1))
        if [ "$POLLS" -gt 600 ]; then
            echo "port: the capture of frame $FRAME is still pending after 30 s" >&2
            return 1
        fi
        sleep 0.05
    done
    if [ "$(printf '%s' "$STATUS" | port_field captureFailures)" != "$FAILURES" ]; then
        echo "port: the capture of frame $FRAME to $1 failed" >&2
        return 1
    fi
}
