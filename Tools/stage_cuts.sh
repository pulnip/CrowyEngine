#!/bin/sh
# Captures StageEditor from every cut of Backlot's scene file in the chosen
# lighting keys, through the command port, and compares each picture with
# its golden: a held loop, one property write per cut and key, one frame
# each. Names follow Backlot's own captures (<cut>.png for the default key,
# <cut>@<key>.png for the others), so a run lines up with
# Backlot/Captures/<run>/ for a placement comparison.
#
# The goldens are Engine/Render/Sample/Golden/StageEditor/<name>.<backend>.png
# (metal on macOS, dx12 elsewhere), taken at the Backlot commit
# StageEditor.backlot.txt names, for the keys it pins; the launch picture's
# golden is the smoke's StageEditor.<backend>.png. A Backlot at another
# commit, or with changes the editor reads, is captured but not compared
# (exit 77). --record copies the pictures that are missing or differ and
# stamps Backlot's commit.
#
# usage: Tools/stage_cuts.sh [--keys day,night] [--cuts street,roof]
#        [--out captures/stage/<stamp>] [--exe build/bin/StageEditor]
#        [--port 27520] [--tolerance N] [--record]
#
# Debug builds only (the port). Run from the repository root. The editor
# listens on its own port, so a sample already running is never driven.
set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$REPO_ROOT/Tools/port.sh"
. "$REPO_ROOT/Tools/stage_editor.sh"

usage() {
    sed -n 's/^# \{0,1\}//; /^usage:/,/^$/p' "$0" | sed '/^$/d' >&2
    exit 2
}

KEYS=""
CUTS=""
OUT=""
EXE="build/bin/StageEditor"
PORT=27520
TOLERANCE=""
RECORD=0
while [ $# -gt 0 ]; do
    case "$1" in
    --record)
        RECORD=1
        shift
        ;;
    --keys | --cuts | --out | --exe | --port | --tolerance)
        [ $# -ge 2 ] || usage
        case "$1" in
        --keys) KEYS="$2" ;;
        --cuts) CUTS="$2" ;;
        --out) OUT="$2" ;;
        --exe) EXE="$2" ;;
        --port) PORT="$2" ;;
        --tolerance) TOLERANCE="$2" ;;
        esac
        shift 2
        ;;
    *)
        usage
        ;;
    esac
done

[ -n "$OUT" ] || OUT="captures/stage/$(date +%Y%m%d-%H%M%S)"
TOOL="$(dirname "$EXE")/ImageCompareCheck"
if [ ! -x "$TOOL" ]; then
    echo "stage_cuts converts and compares with $TOOL; build ImageCompareCheck first" >&2
    exit 1
fi
if [ "$(uname)" = Darwin ]; then
    BACKEND=metal
    OTHER_BACKEND=dx12
else
    BACKEND=dx12
    OTHER_BACKEND=metal
fi
GOLDEN_ROOT="$REPO_ROOT/Engine/Render/Sample/Golden"
STAMP_FILE="$GOLDEN_ROOT/StageEditor.backlot.txt"
HAVE_STAMP=0
stage_stamp "$STAMP_FILE" && HAVE_STAMP=1
COMPARE_ARGS=""
[ -n "$TOLERANCE" ] && COMPARE_ARGS="--tolerance $TOLERANCE"

# whether a JSON number is zero
is_zero() {
    python3 -c 'import sys; sys.exit(0 if float(sys.argv[1]) == 0 else 1)' "$1"
}

short_hash() {
    printf '%s' "$1" | cut -c1-7
}

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
stage_editor_start "$EXE" "$PORT" || exit 1
# the editor's chrome stays out of the pictures
port_set debug showPanel false || exit 1

# a golden is the launch's still scene: the file it read, at time 0
PAUSED=$(port_get editor paused) || exit 1
TIME=$(port_get editor time) || exit 1
REVISION=$(port_get editor revision) || exit 1
if [ "$PAUSED" != true ] || ! is_zero "$TIME" || [ "$REVISION" != 1 ]; then
    echo "a golden needs the launch's still scene (paused $PAUSED, time $TIME, revision $REVISION)" >&2
    exit 1
fi
# the cut the editor launches on, in the default key: the smoke's picture
LAUNCH_NAME=$(port_get editor cut | stage_unquote) || exit 1

ROOT=$(stage_root "$(port_get editor scene | stage_unquote)") || exit 1
backlot_state "$ROOT"
SCENE="$ROOT/Data/scene.json"
SCENE_CUTS=$(python3 -c 'import json, sys; print(" ".join(c["name"] for c in json.load(open(sys.argv[1], encoding="utf-8"))["cameras"]))' "$SCENE") || exit 1
DEFAULT_KEY=$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["default_key"])' "$SCENE") || exit 1
CUTS=$(echo "$CUTS" | tr ',' ' ')
for CUT in $CUTS; do
    case " $SCENE_CUTS " in
    *" $CUT "*) ;;
    *)
        echo "no cut '$CUT' in $ROOT (cuts: $SCENE_CUTS)" >&2
        exit 1
        ;;
    esac
done
[ -n "$CUTS" ] || CUTS="$SCENE_CUTS"
KEYS=$(echo "$KEYS" | tr ',' ' ')
if [ -z "$KEYS" ]; then
    if [ -n "$STAMP_KEYS" ]; then KEYS="$STAMP_KEYS"; else KEYS="$DEFAULT_KEY"; fi
fi
KEYS=$(echo $KEYS)

if [ -n "$BACKLOT_HEAD" ]; then HEAD=$(short_hash "$BACKLOT_HEAD"); else HEAD="an unknown commit"; fi
if [ -n "$BACKLOT_CHANGES" ]; then CLEAN="with changes"; else CLEAN="clean"; fi
echo "backlot: $ROOT at $HEAD, $CLEAN"
SKIP=""
if [ "$RECORD" = 1 ]; then
    if [ -z "$BACKLOT_HEAD" ]; then
        echo "cannot read Backlot's commit at $ROOT, so goldens cannot be stamped" >&2
        exit 1
    fi
    if [ -n "$BACKLOT_CHANGES" ]; then
        echo "cannot stamp goldens from a Backlot with uncommitted changes the editor reads:" >&2
        echo "$BACKLOT_CHANGES" >&2
        exit 1
    fi
    PINNED="$KEYS"
else
    PINNED="$STAMP_KEYS"
    if [ "$HAVE_STAMP" = 0 ]; then
        SKIP="unstamped: $STAMP_FILE does not exist"
    elif [ -z "$BACKLOT_HEAD" ]; then
        SKIP="cannot read Backlot's commit at $ROOT"
    elif [ "$BACKLOT_HEAD" != "$STAMP_BACKLOT" ]; then
        SKIP="Backlot is at $HEAD, the goldens were taken at $(short_hash "$STAMP_BACKLOT")"
    elif [ -n "$BACKLOT_CHANGES" ]; then
        SKIP="Backlot has uncommitted changes the editor reads ($(printf '%s' "$BACKLOT_CHANGES" | tr '\n' ';' | sed 's/;/; /g'))"
    fi
fi

# the launch picture is the smoke's golden; every other one has its own
golden_path() {
    if [ "$1" = "$LAUNCH_NAME" ]; then
        echo "$GOLDEN_ROOT/StageEditor.$BACKEND.png"
    else
        echo "$GOLDEN_ROOT/StageEditor/$1.$BACKEND.png"
    fi
}

CAPTURED=0
COMPARED=0
SAME=0
DIFFERENT=0
MISSING=0
RECORDED=0
for KEY in $KEYS; do
    set_editor key "$KEY" || exit 1
    for CUT in $CUTS; do
        set_editor cut "$CUT" || exit 1
        if [ "$KEY" = "$DEFAULT_KEY" ]; then NAME="$CUT"; else NAME="$CUT@$KEY"; fi
        rm -f "$OUT/$NAME.png" "$OUT/$NAME.bmp" "$OUT/$NAME.diff.png"
        # a key change rebuilds the walker on the next frame: capture the
        # one after it
        FRAME=$(port_frame) || exit 1
        PNG=$(stage_snap "$OUT/$NAME.bmp" "$TOOL" "$((FRAME + 2))") || exit 1
        CAPTURED=$((CAPTURED + 1))
        if [ -n "$SKIP" ]; then
            echo "captured  $NAME"
            continue
        fi
        case " $PINNED " in
        *" $KEY "*) ;;
        *)
            echo "captured  $NAME (capture only: $KEY is not pinned)"
            continue
            ;;
        esac

        GOLDEN=$(golden_path "$NAME")
        SMOKE=0
        [ "$NAME" = "$LAUNCH_NAME" ] && SMOKE=1
        if [ ! -f "$GOLDEN" ]; then
            if [ "$RECORD" = 1 ] && [ "$SMOKE" = 0 ]; then
                mkdir -p "$(dirname "$GOLDEN")"
                cp "$PNG" "$GOLDEN"
                RECORDED=$((RECORDED + 1))
                echo "recorded  $NAME"
                continue
            fi
            MISSING=$((MISSING + 1))
            echo "FAIL: no golden for $NAME on $BACKEND ($GOLDEN)"
            continue
        fi

        COMPARED=$((COMPARED + 1))
        # shellcheck disable=SC2086
        "$TOOL" "$PNG" "$GOLDEN" $COMPARE_ARGS >/dev/null 2>&1
        RESULT=$?
        if [ "$RESULT" = 0 ]; then
            SAME=$((SAME + 1))
            echo "same      $NAME"
            continue
        fi
        if [ "$RESULT" = 1 ] && [ "$RECORD" = 1 ] && [ "$SMOKE" = 0 ]; then
            cp "$PNG" "$GOLDEN"
            RECORDED=$((RECORDED + 1))
            echo "recorded  $NAME (it differed)"
            continue
        fi
        DIFFERENT=$((DIFFERENT + 1))
        if [ "$RESULT" != 1 ]; then
            echo "FAIL: could not compare $NAME against $GOLDEN"
            continue
        fi
        echo "FAIL: $NAME differs from $GOLDEN"
        # shellcheck disable=SC2086
        "$TOOL" "$PNG" "$GOLDEN" $COMPARE_ARGS --diff "$OUT/$NAME.diff.png"
        echo "diff: $OUT/$NAME.diff.png"
        if [ "$SMOKE" = 1 ]; then
            echo "this is the smoke's golden: if StageEditorSmoke passes, the port's held frames and the free-running frame 60 draw different pictures; accept through Tools/smoke_run.sh build/bin/StageEditor"
        else
            echo "to accept: cp \"$PNG\" \"$GOLDEN\""
        fi
    done
done

TIME=$(port_get editor time) || exit 1
if ! is_zero "$TIME"; then
    echo "scene time moved during the run" >&2
    exit 1
fi

if [ "$RECORD" = 1 ]; then
    if [ "$HAVE_STAMP" = 0 ] || [ "$STAMP_BACKLOT" != "$BACKLOT_HEAD" ] || [ "$(echo $STAMP_KEYS)" != "$KEYS" ]; then
        stage_stamp_write "$STAMP_FILE" "$BACKLOT_HEAD" "$KEYS"
        echo "stamp: backlot $BACKLOT_HEAD, keys $KEYS"
    fi
    # the other backend's pictures were taken at the old commit
    if [ "$HAVE_STAMP" = 1 ] && [ "$STAMP_BACKLOT" != "$BACKLOT_HEAD" ] && ls "$GOLDEN_ROOT"/StageEditor/*."$OTHER_BACKEND".png >/dev/null 2>&1; then
        echo "note: every StageEditor/*.$OTHER_BACKEND.png is stale until it is recorded again on its backend"
    fi
fi

if [ -n "$TOLERANCE" ]; then TOLERANCE_TEXT="$TOLERANCE"; else TOLERANCE_TEXT="the tool's default"; fi
RECORDED_TEXT=""
[ "$RECORD" = 1 ] && RECORDED_TEXT=", $RECORDED recorded"
echo "stage_cuts: $CAPTURED captured, $COMPARED compared, $SAME same, $DIFFERENT different, $MISSING missing$RECORDED_TEXT; $BACKEND, tolerance $TOLERANCE_TEXT"
STATUS=0
if [ -n "$SKIP" ]; then
    echo "SKIP: $SKIP; captured into $OUT, compared nothing. Once the new pictures are right: Tools/stage_cuts.sh --record"
    STATUS=77
elif [ "$DIFFERENT" -gt 0 ] || [ "$MISSING" -gt 0 ]; then
    [ "$MISSING" -gt 0 ] && echo "to record the missing goldens: Tools/stage_cuts.sh --record"
    STATUS=1
fi
echo "cuts: $OUT"
exit "$STATUS"
