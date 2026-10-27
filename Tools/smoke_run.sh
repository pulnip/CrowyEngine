#!/bin/sh
# Run one executable the way its ctest entry does: launch it, let it render
# a counted number of frames and exit by itself, then fail on crashes,
# thrown exceptions, or Metal validation errors.
#
# usage: smoke_run.sh <executable> [capture-image-path]
#
# The contract (frames, timeout, captures, goldens, the 77 skip) is
# docs/testing.md "Windowed runs and goldens". Run from the repository root:
# samples load Engine/Shader and Content by relative path.
set -u

APP="$1"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME="$(basename "$APP")"
TOOL="$(dirname "$APP")/ImageCompareCheck"
if [ "$(uname)" = "Darwin" ]; then
    BACKEND=metal
else
    BACKEND=dx12
fi

CAPTURE="${2:-}"
if [ -z "$CAPTURE" ] && [ -n "${CROWY_SMOKE_CAPTURE_DIR:-}" ]; then
    CAPTURE="$CROWY_SMOKE_CAPTURE_DIR/$NAME.bmp"
fi

# a spike's golden, found before the launch so the run captures its frame
GOLDEN=""
for CANDIDATE in "$REPO_ROOT"/Engine/*/Spike/Golden/"$NAME.$BACKEND.png"; do
    if [ -f "$CANDIDATE" ]; then
        GOLDEN="$CANDIDATE"
        break
    fi
done
EXPECTED_GOLDEN=""
if [ "${CROWY_SMOKE_CAPTURE_AT:-60}" = 60 ]; then
    EXPECTED_GOLDEN="$GOLDEN"
fi
# a golden gates every run, not only runs that collect captures
if [ -z "$CAPTURE" ] && [ -n "$EXPECTED_GOLDEN" ]; then
    CAPTURE="${TMPDIR:-/tmp}/crowy-smoke/$NAME.bmp"
fi
MUST_EXIT=0
[ "${CROWY_SMOKE_MUST_EXIT:-}" = 1 ] && MUST_EXIT=1
CAPTURE_AT="${CROWY_SMOKE_CAPTURE_AT:-60}"
FRAMES="${CROWY_SMOKE_FRAMES:-60}"
# a capture asks for its frame, so the run reaches it
if [ -n "$CAPTURE" ] && [ "$CAPTURE_AT" -gt "$FRAMES" ]; then
    FRAMES="$CAPTURE_AT"
fi
WATCH="${CROWY_SMOKE_TIMEOUT:-60}"

LOG="${TMPDIR:-/tmp}/crowy-smoke-$$.log"
trap 'rm -f "$LOG"' EXIT

if [ "$(uname)" = "Darwin" ]; then
    # warnings go to the log; errors keep the default assert mode,
    # which aborts the app and turns into a nonzero exit below
    export MTL_DEBUG_LAYER=1
    export MTL_DEBUG_LAYER_WARNING_MODE=nslog
fi

if [ -n "$CAPTURE" ]; then
    mkdir -p "$(dirname "$CAPTURE")"
    # an earlier run's PNG or heat map must not read as this run's
    rm -f "$CAPTURE" "${CAPTURE%.*}.diff.png"
    case "$CAPTURE" in
    *.bmp) rm -f "${CAPTURE%.bmp}.png" ;;
    esac
fi

# a check is headless and may parse arguments of its own
set --
if [ "$MUST_EXIT" -eq 0 ]; then
    set -- --exit-after-frame "$FRAMES"
    if [ -n "$CAPTURE" ]; then
        set -- "$@" --capture-frame "$CAPTURE_AT" "$CAPTURE"
    fi
fi

"$APP" "$@" >"$LOG" 2>&1 &
PID=$!

STATUS=0
EXITED=0
ELAPSED=0
while [ "$ELAPSED" -lt "$WATCH" ]; do
    if ! kill -0 "$PID" 2>/dev/null; then
        EXITED=1
        break
    fi
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

if [ "$EXITED" -eq 1 ]; then
    wait "$PID"
    STATUS=$?
    if [ "$STATUS" -eq 77 ]; then
        echo "SKIP: the sample reported its content missing" >&2
        tail -n 5 "$LOG" >&2
        exit 77
    fi
    if [ "$STATUS" -ne 0 ]; then
        echo "FAIL: exited early with status $STATUS" >&2
    fi
else
    kill "$PID" 2>/dev/null
    wait "$PID" 2>/dev/null
    if [ "$MUST_EXIT" -eq 1 ]; then
        echo "FAIL: did not exit within $WATCH s" >&2
    else
        echo "FAIL: did not exit after frame $FRAMES within $WATCH s" >&2
    fi
    STATUS=1
fi

if grep -q "failed assertion\|Draw Errors\|terminating\|Assertion failed" "$LOG"; then
    echo "FAIL: assertion or validation error" >&2
    STATUS=1
fi

if [ "$STATUS" -ne 0 ]; then
    echo "--- last log lines ---" >&2
    tail -n 40 "$LOG" >&2
    exit 1
fi

if [ -z "$CAPTURE" ]; then
    exit 0
fi
if [ ! -f "$CAPTURE" ]; then
    if [ -n "$EXPECTED_GOLDEN" ]; then
        echo "FAIL: $EXPECTED_GOLDEN exists, but frame 60 was not captured" >&2
        exit 1
    fi
    # headless samples have no swapchain, so nothing to dump
    echo "note: no frame captured (sample presented no frame?)" >&2
    exit 0
fi

# uncompressed BMP is bulky
case "$CAPTURE" in
*.bmp)
    PNG="${CAPTURE%.bmp}.png"
    if [ -x "$TOOL" ] && "$TOOL" --convert "$CAPTURE" "$PNG"; then
        rm -f "$CAPTURE"
        CAPTURE="$PNG"
    fi
    ;;
esac
echo "captured frame: $CAPTURE"

if [ -z "$GOLDEN" ]; then
    exit 0
fi

if [ "$CAPTURE_AT" != 60 ]; then
    echo "note: not compared with $GOLDEN (captured frame $CAPTURE_AT, the golden is frame 60)" >&2
    exit 0
fi
if [ ! -x "$TOOL" ]; then
    echo "FAIL: $GOLDEN exists, but $TOOL is not built" >&2
    exit 1
fi

"$TOOL" "$CAPTURE" "$GOLDEN"
case $? in
0)
    exit 0
    ;;
1)
    DIFF="${CAPTURE%.*}.diff.png"
    echo "FAIL: capture differs from $GOLDEN" >&2
    # a passing run leaves no heat map, so only a difference writes one
    "$TOOL" "$CAPTURE" "$GOLDEN" --diff "$DIFF" >/dev/null
    if [ -f "$DIFF" ]; then
        echo "diff: $DIFF" >&2
    fi
    echo "to accept: cp \"$CAPTURE\" \"$GOLDEN\"" >&2
    exit 1
    ;;
*)
    echo "FAIL: could not compare against $GOLDEN" >&2
    exit 1
    ;;
esac
