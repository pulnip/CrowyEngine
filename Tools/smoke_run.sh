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
# capture saves one rendered frame. Either pass a path as the third
# argument, or set $CROWY_SMOKE_CAPTURE_DIR to collect <sample>.png per
# sample:
#   CROWY_SMOKE_CAPTURE_DIR=captures ctest --test-dir build -C Debug -L smoke
# It rides on the engine's CROWY_DUMP_FRAME hook, which writes a BMP; the
# captured frame index can be overridden with $CROWY_SMOKE_CAPTURE_AT.
# A .bmp capture becomes a PNG through ImageCompareCheck beside the
# executable (the BMP stays when that tool is not built).
#
# golden: when Engine/*/Sample/Golden/<sample>.metal.png (or Spike/Golden)
# exists, a capture of the default frame 60 is compared against it with
# ImageCompareCheck's defaults, and a difference fails the run with a heat
# map beside the capture. To accept a new picture, copy the capture over
# the golden; the failure prints the command. With such a golden the run
# waits past the duration until frame 60's capture is complete, up to
# $CROWY_SMOKE_CAPTURE_TIMEOUT seconds (60), and fails when none lands: a
# golden never gates on a frame it did not compare.
#
# An exit status of 77 is a skip (the sample's content is missing) and
# passes through for ctest's SKIP_RETURN_CODE.
#
# Run from the repository root: samples load Engine/Shader and Content
# by relative path.
set -u

APP="$1"
DURATION="${2:-${CROWY_SMOKE_DURATION:-5}}"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME="$(basename "$APP")"
TOOL="$(dirname "$APP")/ImageCompareCheck"
BACKEND=metal

CAPTURE="${3:-}"
if [ -z "$CAPTURE" ] && [ -n "${CROWY_SMOKE_CAPTURE_DIR:-}" ]; then
    CAPTURE="$CROWY_SMOKE_CAPTURE_DIR/$NAME.bmp"
fi

# the golden a frame-60 capture is compared with, found before the launch
# so the watch can wait for that capture
EXPECTED_GOLDEN=""
if [ -n "$CAPTURE" ] && [ "${CROWY_SMOKE_CAPTURE_AT:-60}" = 60 ]; then
    for CANDIDATE in \
        "$REPO_ROOT"/Engine/*/Sample/Golden/"$NAME.$BACKEND.png" \
        "$REPO_ROOT"/Engine/*/Spike/Golden/"$NAME.$BACKEND.png"; do
        if [ -f "$CANDIDATE" ]; then
            EXPECTED_GOLDEN="$CANDIDATE"
            break
        fi
    done
fi
CAPTURE_TIMEOUT="${CROWY_SMOKE_CAPTURE_TIMEOUT:-60}"

# a dump is written on a thread: complete once the BMP header's file size
# (bytes 2-5, little-endian) matches the bytes on disk
capture_complete() {
    [ -f "$1" ] || return 1
    HEADER_SIZE=$(od -An -t u4 -j 2 -N 4 "$1" 2>/dev/null | tr -d ' ')
    [ -n "$HEADER_SIZE" ] && [ "$HEADER_SIZE" -eq "$(wc -c <"$1" | tr -d ' ')" ]
}

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

if [ "$EXITED" -eq 0 ] && [ -n "$EXPECTED_GOLDEN" ]; then
    case "$CAPTURE" in
    *.bmp)
        # a slow start has not reached frame 60 yet: wait for its capture
        WAITED=0
        while ! capture_complete "$CAPTURE" && [ "$WAITED" -lt "$CAPTURE_TIMEOUT" ]; do
            if ! kill -0 "$PID" 2>/dev/null; then
                EXITED=1
                break
            fi
            sleep 1
            WAITED=$((WAITED + 1))
        done
        if [ "$WAITED" -gt 0 ]; then
            echo "waited $WAITED s past the duration for frame 60"
        fi
        ;;
    esac
fi

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

if [ -z "$CAPTURE" ]; then
    exit 0
fi
if [ ! -f "$CAPTURE" ]; then
    if [ -n "$EXPECTED_GOLDEN" ]; then
        echo "FAIL: $EXPECTED_GOLDEN exists, but frame 60 was not captured within $((DURATION + CAPTURE_TIMEOUT)) s" >&2
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

GOLDEN=""
for CANDIDATE in \
    "$REPO_ROOT"/Engine/*/Sample/Golden/"$NAME.$BACKEND.png" \
    "$REPO_ROOT"/Engine/*/Spike/Golden/"$NAME.$BACKEND.png"; do
    if [ -f "$CANDIDATE" ]; then
        GOLDEN="$CANDIDATE"
        break
    fi
done
if [ -z "$GOLDEN" ]; then
    exit 0
fi

if [ "${CROWY_SMOKE_CAPTURE_AT:-60}" != 60 ]; then
    echo "note: not compared with $GOLDEN (captured frame $CROWY_SMOKE_CAPTURE_AT, the golden is frame 60)" >&2
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
