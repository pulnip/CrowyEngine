#!/bin/sh
# Drive a benchmark: run one or two samples alternately, and file away what
# each run wrote. The frame counts, warmup and output paths live in the
# --config document, so this script only orchestrates and collects.
#
# Alternating A and B is the point when comparing two builds or two APIs:
# it cancels the thermal drift and background load that a run of five A's
# followed by five B's would bake into the second half.
#
# usage: bench_run.sh [app-a] [app-b] [reps] [stage-dir] [out-dir] [config]
#
# Every argument falls back to the default below. The sample must be a
# Debug or CROWY_BENCHMARK build, run with --config; the config's report and
# frame paths must point into stage-dir.
#
# Run from the repository root: samples load Engine/Shader and Content by
# relative path.
set -u

APP_A="${1:-}"
APP_B="${2:-}"
REPS="${3:-5}"
# where the samples write their report and CSV
STAGE_DIR="${4:-bench}"
# where this script keeps each run
OUT_DIR="${5:-bench-runs}"
# the app document every run is started with
CONFIG="${6:-Tools/bench.app.json}"
TIMEOUT="${CROWY_BENCH_TIMEOUT:-300}"

APPS=""
for APP in "$APP_A" "$APP_B"; do
    [ -z "$APP" ] && continue

    if [ ! -x "$APP" ]; then
        echo "FAIL: no such executable: $APP" >&2
        exit 1
    fi
    APPS="$APPS $APP"
done

if [ -z "$APPS" ]; then
    echo "FAIL: give at least one executable" >&2
    exit 1
fi

if [ ! -f "$CONFIG" ]; then
    echo "FAIL: no such config: $CONFIG" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

REP=1
while [ "$REP" -le "$REPS" ]; do
    for APP in $APPS; do
        LABEL="$(basename "$APP")-rep$REP"

        # start from an empty stage so the collection below cannot pick up
        # anything an earlier run left behind
        rm -rf "$STAGE_DIR"
        mkdir -p "$STAGE_DIR"

        echo "running $LABEL ..."
        "$APP" --config "$CONFIG" &
        PID=$!

        ELAPSED=0
        while [ "$ELAPSED" -lt "$TIMEOUT" ] && kill -0 "$PID" 2>/dev/null; do
            sleep 1
            ELAPSED=$((ELAPSED + 1))
        done

        if kill -0 "$PID" 2>/dev/null; then
            kill "$PID" 2>/dev/null
            wait "$PID" 2>/dev/null

            echo "FAIL: $LABEL ran past ${TIMEOUT}s." >&2
            echo "      is this a Debug or CROWY_BENCHMARK build, run with --config?" >&2
            exit 1
        fi

        wait "$PID"
        STATUS=$?
        if [ "$STATUS" -ne 0 ]; then
            echo "FAIL: $LABEL exited with status $STATUS" >&2
            exit 1
        fi

        if [ -z "$(ls -A "$STAGE_DIR" 2>/dev/null)" ]; then
            echo "FAIL: $LABEL wrote nothing into $STAGE_DIR" >&2
            echo "      do $CONFIG's report_path and frame_path point in there?" >&2
            exit 1
        fi

        RUN_DIR="$OUT_DIR/$LABEL"
        mkdir -p "$RUN_DIR"
        mv "$STAGE_DIR"/* "$RUN_DIR"/

        echo "  -> $RUN_DIR"
    done
    REP=$((REP + 1))
done

rm -rf "$STAGE_DIR"
echo "done: $REPS rep(s) under $OUT_DIR"
exit 0
