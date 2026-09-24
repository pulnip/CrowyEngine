#!/bin/sh
# Build CrowyEngine through the CMake presets, the same way every time.
#
# The macOS counterpart of Tools/build.ps1: the same meaning, defaults and log
# files under build/, with sh-style flags. A hand-written cmake or ninja
# command skips the configure-when-missing step and the toolchain check below,
# and a piped build buffers its output until it looks hung, so everything goes
# to log files and nothing is piped.
#
# --detach starts a copy of this script in its own session, so a long build
# outlives a shell timeout. Poll it with --status.
#
# usage: Tools/build.sh [--config Debug|RelWithDebInfo|Release] [--target <name>]
#                       [--preset <configure-preset>] [--clean] [--fresh]
#                       [--detach] [--status] [--log-file <path>]
#
#   Tools/build.sh --target CrowyRenderTest             # foreground
#   Tools/build.sh --target CrowyRenderTest --detach    # start and return
#   Tools/build.sh --status                             # running? / exit code / log tail
#
# One target per call and one build at a time; never kill a running build.
set -u

PRESET="macOS-brew-llvm"
CONFIG="Debug"
TARGET=""
CLEAN=0
FRESH=0
DETACH=0
STATUS=0
LOG_FILE=""

usage() {
    echo "usage: Tools/build.sh [--config Debug|RelWithDebInfo|Release] [--target <name>]" >&2
    echo "                      [--preset <configure-preset>] [--clean] [--fresh]" >&2
    echo "                      [--detach] [--status] [--log-file <path>]" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
    --preset)
        [ $# -ge 2 ] || usage
        PRESET="$2"
        shift 2
        ;;
    --config)
        [ $# -ge 2 ] || usage
        CONFIG="$2"
        shift 2
        ;;
    --target)
        [ $# -ge 2 ] || usage
        if [ -n "$TARGET" ]; then
            echo "FAIL: one target per call" >&2
            exit 2
        fi
        TARGET="$2"
        shift 2
        ;;
    --clean)
        CLEAN=1
        shift
        ;;
    --fresh)
        FRESH=1
        shift
        ;;
    --detach)
        DETACH=1
        shift
        ;;
    --status)
        STATUS=1
        shift
        ;;
    --log-file)
        [ $# -ge 2 ] || usage
        LOG_FILE="$2"
        shift 2
        ;;
    *)
        echo "FAIL: unknown argument '$1'" >&2
        usage
        ;;
    esac
done

case "$CONFIG" in
Debug|RelWithDebInfo|Release) ;;
*)
    echo "FAIL: --config must be Debug, RelWithDebInfo or Release" >&2
    exit 2
    ;;
esac

case "$TARGET" in
*,*|*" "*)
    echo "FAIL: one target per call" >&2
    exit 2
    ;;
esac

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SCRIPT="$REPO_ROOT/Tools/build.sh"
BINARY_DIR="$REPO_ROOT/build"

CUSTOM_LOG="$LOG_FILE"
[ -n "$LOG_FILE" ] || LOG_FILE="$BINARY_DIR/build-log.txt"
CONFIGURE_LOG="$BINARY_DIR/configure-log.txt"
SUMMARY_FILE="$BINARY_DIR/build-summary.txt"
PID_FILE="$BINARY_DIR/build.pid"
STATUS_FILE="$BINARY_DIR/build-status.txt"

mkdir -p "$BINARY_DIR"

# A PID alone is not enough: once a build is gone, an unrelated process can
# be handed the same number.
isRunning() {
    [ -f "$PID_FILE" ] || return 1

    ps -p "$(cat "$PID_FILE")" -o command= 2>/dev/null | grep -q "build.sh"
}

# ------------------------------------------------------------------ --status
if [ "$STATUS" -eq 1 ]; then
    if isRunning; then
        echo "running : true"
    else
        echo "running : false"
    fi
    if [ -f "$STATUS_FILE" ]; then
        echo "status  : $(cat "$STATUS_FILE")"
    fi

    LIVE=""
    if [ -f "$LOG_FILE" ]; then
        LIVE="$LOG_FILE"
    elif [ -f "$CONFIGURE_LOG" ]; then
        LIVE="$CONFIGURE_LOG"
    fi
    if [ -n "$LIVE" ]; then
        echo "log     : $LIVE  (updated $(date -r "$LIVE"))"
        echo "--- tail ---"
        tail -n 15 "$LIVE"
    fi
    if ! isRunning && [ -f "$SUMMARY_FILE" ]; then
        echo "--- summary ---"
        cat "$SUMMARY_FILE"
    fi
    exit 0
fi

# ------------------------------------------------------------------ --detach
# setsid gives the copy its own session and process group, so whatever ends
# the caller's group misses it. macOS ships no setsid(1); perl reaches the
# syscall, and nohup alone covers the hangup when perl is missing.
if [ "$DETACH" -eq 1 ]; then
    if isRunning; then
        echo "A build is already running (PID $(cat "$PID_FILE")). Exactly one build at a time - wait for it."
        exit 1
    fi
    rm -f "$STATUS_FILE"

    set -- --preset "$PRESET" --config "$CONFIG"
    if [ -n "$TARGET" ]; then
        set -- "$@" --target "$TARGET"
    fi
    if [ "$CLEAN" -eq 1 ]; then
        set -- "$@" --clean
    fi
    if [ "$FRESH" -eq 1 ]; then
        set -- "$@" --fresh
    fi
    if [ -n "$CUSTOM_LOG" ]; then
        set -- "$@" --log-file "$CUSTOM_LOG"
    fi

    cd "$REPO_ROOT" || exit 1
    if command -v perl >/dev/null 2>&1; then
        nohup perl -MPOSIX -e 'POSIX::setsid(); exec @ARGV' -- sh "$SCRIPT" "$@" \
            >"$SUMMARY_FILE" 2>&1 </dev/null &
    else
        nohup sh "$SCRIPT" "$@" >"$SUMMARY_FILE" 2>&1 </dev/null &
    fi

    # the copy records itself too; writing it here as well closes the gap
    # before it starts, so an immediate --status already sees it
    echo $! >"$PID_FILE"
    echo "detached build started (PID $!)"
    echo "poll with : Tools/build.sh --status"
    exit 0
fi

# ------------------------------------------------------------------- the run
if isRunning && [ "$(cat "$PID_FILE")" != "$$" ]; then
    echo "A build is already running (PID $(cat "$PID_FILE")). Exactly one build at a time - wait for it."
    exit 1
fi
echo $$ >"$PID_FILE"
rm -f "$STATUS_FILE"

# The toolchain file asks brew where LLVM lives and, when brew cannot be run,
# silently leaves CMake on AppleClang: a different compiler, and no error.
LLVM_PREFIX=""
case "$PRESET" in
macOS-brew-llvm*)
    if ! command -v brew >/dev/null 2>&1; then
        echo "FAIL: brew is not on PATH, so the brew-llvm toolchain would configure AppleClang" >&2
        echo "configure FAILED (no brew)" >"$STATUS_FILE"
        exit 1
    fi
    LLVM_PREFIX="$(brew --prefix llvm)"
    ;;
esac

echo "repo    : $REPO_ROOT"
echo ""
START=$(date +%s)

# Configure when the cache is missing (fresh clone, or build/ was deleted);
# a bare `cmake --build --preset` fails with "could not load cache".
if [ "$FRESH" -eq 1 ] || [ ! -f "$BINARY_DIR/CMakeCache.txt" ]; then
    set -- --preset "$PRESET"
    if [ "$FRESH" -eq 1 ]; then
        set -- "$@" --fresh
    fi

    echo "=== configure ==="
    echo "command : cmake $*"
    echo "log     : $CONFIGURE_LOG"
    (cd "$REPO_ROOT" && cmake "$@") >"$CONFIGURE_LOG" 2>&1
    CODE=$?
    echo "configure exit code : $CODE  ($(($(date +%s) - START)) s)"
    echo ""
    if [ "$CODE" -ne 0 ]; then
        echo "--- configure log tail ---"
        tail -n 40 "$CONFIGURE_LOG"
        echo "configure FAILED ($CODE)" >"$STATUS_FILE"
        exit "$CODE"
    fi
fi

# A cache configured before brew was on PATH keeps its AppleClang.
if [ -n "$LLVM_PREFIX" ]; then
    CMAKE_VERSION="$(cmake --version | sed -n '1s/^cmake version //p')"
    COMPILER_FILE="$BINARY_DIR/CMakeFiles/$CMAKE_VERSION/CMakeCXXCompiler.cmake"
    if [ -f "$COMPILER_FILE" ] &&
        ! grep -q "set(CMAKE_CXX_COMPILER \"$LLVM_PREFIX/" "$COMPILER_FILE"; then
        echo "FAIL: build/ was configured with a compiler outside $LLVM_PREFIX - rerun with --fresh" >&2
        grep "set(CMAKE_CXX_COMPILER " "$COMPILER_FILE" >&2
        echo "configure FAILED (not brew clang)" >"$STATUS_FILE"
        exit 1
    fi
fi

# Build preset names are "<configure-preset>-<Config>".
set -- --build --preset "$PRESET-$CONFIG"
if [ "$CLEAN" -eq 1 ]; then
    set -- "$@" --clean-first
fi
if [ -n "$TARGET" ]; then
    set -- "$@" --target "$TARGET"
fi

echo "=== build ==="
echo "command : cmake $*"
echo "log     : $LOG_FILE"
(cd "$REPO_ROOT" && cmake "$@") >"$LOG_FILE" 2>&1
EXIT_CODE=$?
ELAPSED=$(($(date +%s) - START))

# 'error:' / 'FAILED:' with the colon - a bare 'error' also matches source
# file names like SDL_error.c.o scrolling past in the log
ERRORS=$(grep -c -F -e 'error:' -e 'FAILED:' "$LOG_FILE")

echo ""
echo "exit code : $EXIT_CODE"
echo "elapsed   : $ELAPSED s (configure + build)"
echo "error/FAILED lines in log : $ERRORS"
if [ "$ERRORS" -gt 0 ]; then
    echo "--- first 40 ---"
    grep -F -e 'error:' -e 'FAILED:' "$LOG_FILE" | head -n 40
fi

echo "exit $EXIT_CODE after $ELAPSED s" >"$STATUS_FILE"
exit "$EXIT_CODE"
