#!/bin/sh
# Smoke-run a sample: launch it, let it render for a while,
# then fail on crashes, thrown exceptions, or Metal validation errors.
# An early exit with status 0 counts as success (e.g. HelloCompute).
#
# usage: smoke_run.sh <executable> [duration-seconds] [capture-image-path]
#
# duration falls back to $CROWY_SMOKE_DURATION, then 5 seconds, so a
# ctest run can be stretched without reconfiguring:
#   CROWY_SMOKE_DURATION=15 ctest --test-dir build -C Debug -L smoke
#
# capture saves one rendered frame as an image (PNG when sips can
# convert, PPM otherwise). Either pass a path as the third argument, or
# set $CROWY_SMOKE_CAPTURE_DIR to collect <sample>.png per sample:
#   CROWY_SMOKE_CAPTURE_DIR=captures ctest --test-dir build -C Debug -L smoke
# It rides on the engine's CROWY_DUMP_FRAME hook (Metal only); the
# captured frame index can be overridden with $CROWY_SMOKE_CAPTURE_AT.
#
# Run from the repository root: samples load Engine/Shader and Content
# by relative path.
set -u

APP="$1"
DURATION="${2:-${CROWY_SMOKE_DURATION:-5}}"

CAPTURE="${3:-}"
if [ -z "$CAPTURE" ] && [ -n "${CROWY_SMOKE_CAPTURE_DIR:-}" ]; then
    CAPTURE="$CROWY_SMOKE_CAPTURE_DIR/$(basename "$APP").ppm"
fi

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
    rm -f "$CAPTURE"
    export CROWY_DUMP_FRAME="$CAPTURE"
    if [ -n "${CROWY_SMOKE_CAPTURE_AT:-}" ]; then
        export CROWY_DUMP_FRAME_AT="$CROWY_SMOKE_CAPTURE_AT"
    fi
fi

"$APP" >"$LOG" 2>&1 &
PID=$!

STATUS=0
EXITED=0
ELAPSED=0
while [ "$ELAPSED" -lt "$DURATION" ]; do
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
    if [ "$STATUS" -ne 0 ]; then
        echo "FAIL: exited early with status $STATUS" >&2
    fi
else
    # still alive after the watch window: healthy. shut it down.
    kill "$PID" 2>/dev/null
    wait "$PID" 2>/dev/null
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

if [ -n "$CAPTURE" ]; then
    if [ -f "$CAPTURE" ]; then
        # PPM is bulky and awkward to preview; convert when sips is around
        case "$CAPTURE" in
        *.ppm)
            PNG="${CAPTURE%.ppm}.png"
            if sips -s format png "$CAPTURE" --out "$PNG" >/dev/null 2>&1; then
                rm -f "$CAPTURE"
                CAPTURE="$PNG"
            fi
            ;;
        esac
        echo "captured frame: $CAPTURE"
    else
        # headless samples have no swapchain, so nothing to dump
        echo "note: no frame captured (sample presented no frame?)" >&2
    fi
fi

exit 0
