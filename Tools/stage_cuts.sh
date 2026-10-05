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
# --unity-run <run> sets each picture against Unity's of the same name in
# <Backlot>/Captures/<run> (or a folder given as a path) by their edges,
# report only: <out>/unity holds an overlay per picture, summary.tsv and
# report.html. Style differs by design; the rows point at placement and
# never change the exit status.
#
# usage: Tools/stage_cuts.sh [--keys day,night] [--cuts street,roof]
#        [--out captures/stage/<stamp>] [--exe build/bin/StageEditor]
#        [--port 27520] [--tolerance N] [--record] [--unity-run <run>]
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
UNITY_RUN=""
while [ $# -gt 0 ]; do
    case "$1" in
    --record)
        RECORD=1
        shift
        ;;
    --keys | --cuts | --out | --exe | --port | --tolerance | --unity-run)
        [ $# -ge 2 ] || usage
        case "$1" in
        --keys) KEYS="$2" ;;
        --cuts) CUTS="$2" ;;
        --out) OUT="$2" ;;
        --exe) EXE="$2" ;;
        --port) PORT="$2" ;;
        --tolerance) TOLERANCE="$2" ;;
        --unity-run) UNITY_RUN="$2" ;;
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

# remember <golden status>: the capture just taken, for the Unity rows
CAPTURES=""
TAB=$(printf '\t')
remember() {
    CAPTURES="$CAPTURES$NAME$TAB$CUT$TAB$KEY$TAB$PNG$TAB$1
"
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
UNITY_DIR=""
if [ -n "$UNITY_RUN" ]; then
    if [ -d "$UNITY_RUN" ]; then UNITY_DIR="$(cd "$UNITY_RUN" && pwd)"; else UNITY_DIR="$ROOT/Captures/$UNITY_RUN"; fi
    if [ ! -d "$UNITY_DIR" ]; then
        echo "unity: no run at $UNITY_DIR; the rows are skipped"
        UNITY_DIR=""
    fi
fi
SKIP=""
SKIP_HINT="Once the new pictures are right: Tools/stage_cuts.sh --record"
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
        SKIP_HINT="Goldens are stamped only from a git checkout of Backlot."
    elif [ "$BACKLOT_HEAD" != "$STAMP_BACKLOT" ]; then
        SKIP="Backlot is at $HEAD, the goldens were taken at $(short_hash "$STAMP_BACKLOT")"
    elif [ -n "$BACKLOT_CHANGES" ]; then
        SKIP="Backlot has uncommitted changes the editor reads ($(printf '%s' "$BACKLOT_CHANGES" | tr '\n' ';' | sed 's/;/; /g'))"
        SKIP_HINT="Commit or revert them in Backlot first."
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
SMOKE_MISSING=0
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
            remember skipped
            echo "captured  $NAME"
            continue
        fi
        case " $PINNED " in
        *" $KEY "*) ;;
        *)
            remember "not pinned"
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
                remember recorded
                echo "recorded  $NAME"
                continue
            fi
            MISSING=$((MISSING + 1))
            remember missing
            echo "FAIL: no golden for $NAME on $BACKEND ($GOLDEN)"
            if [ "$SMOKE" = 1 ]; then
                SMOKE_MISSING=1
                echo "this is the smoke's golden, which --record never writes: capture it with CROWY_SMOKE_CAPTURE_DIR set through Tools/smoke_run.sh build/bin/StageEditor and copy the capture to $GOLDEN"
            fi
            continue
        fi

        COMPARED=$((COMPARED + 1))
        # shellcheck disable=SC2086
        "$TOOL" "$PNG" "$GOLDEN" $COMPARE_ARGS >/dev/null 2>&1
        RESULT=$?
        if [ "$RESULT" = 0 ]; then
            SAME=$((SAME + 1))
            remember same
            echo "same      $NAME"
            continue
        fi
        if [ "$RESULT" = 1 ] && [ "$RECORD" = 1 ] && [ "$SMOKE" = 0 ]; then
            cp "$PNG" "$GOLDEN"
            RECORDED=$((RECORDED + 1))
            remember recorded
            echo "recorded  $NAME (it differed)"
            continue
        fi
        DIFFERENT=$((DIFFERENT + 1))
        remember differs
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
    # the stamp keeps every key it pinned; a run adds its own after them
    STAMPED_KEYS=$(echo $STAMP_KEYS)
    for KEY in $KEYS; do
        case " $STAMPED_KEYS " in
        *" $KEY "*) ;;
        *) STAMPED_KEYS=$(echo $STAMPED_KEYS "$KEY") ;;
        esac
    done
    MOVED=0
    [ "$HAVE_STAMP" = 1 ] && [ "$STAMP_BACKLOT" != "$BACKLOT_HEAD" ] && MOVED=1
    if [ "$HAVE_STAMP" = 0 ] || [ "$MOVED" = 1 ] || [ "$(echo $STAMP_KEYS)" != "$STAMPED_KEYS" ]; then
        stage_stamp_write "$STAMP_FILE" "$BACKLOT_HEAD" "$STAMPED_KEYS"
        echo "stamp: backlot $BACKLOT_HEAD, keys $STAMPED_KEYS"
    fi
    UNRECORDED=0
    for KEY in $STAMPED_KEYS; do
        case " $KEYS " in
        *" $KEY "*) ;;
        *) UNRECORDED=1 ;;
        esac
    done
    if [ "$MOVED" = 1 ] && { [ "$UNRECORDED" = 1 ] || [ "$(echo $CUTS | wc -w)" -lt "$(echo $SCENE_CUTS | wc -w)" ]; }; then
        echo "note: the goldens this run did not record were taken at $(short_hash "$STAMP_BACKLOT"); record them at this commit too"
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
    echo "SKIP: $SKIP; captured into $OUT, compared nothing. $SKIP_HINT"
    STATUS=77
elif [ "$DIFFERENT" -gt 0 ] || [ "$MISSING" -gt 0 ]; then
    [ "$MISSING" -gt "$SMOKE_MISSING" ] && echo "to record the missing goldens: Tools/stage_cuts.sh --record"
    STATUS=1
elif [ "$RECORD" = 0 ] && [ "$COMPARED" = 0 ]; then
    echo "SKIP: no key of this run is pinned (pinned: $(echo $PINNED)); captured into $OUT, compared nothing"
    STATUS=77
fi
echo "cuts: $OUT"

if [ -n "$UNITY_DIR" ]; then
    # the editor is done; the rows need only the pictures
    stage_editor_stop
    trap - EXIT
    UNITY_OUT="$OUT/unity"
    mkdir -p "$UNITY_OUT"
    SUMMARY="$UNITY_OUT/summary.tsv"
    printf 'name\tcut\tkey\tgolden\tunity\tedges_engine\tedges_unity\tengine_near_unity\tunity_near_engine\tworst_tile\tworst_tile_near\tworst_tile_edges\tshift\tshift_coincide\tzero_coincide\n' >"$SUMMARY"
    printf '%s' "$CAPTURES" | while IFS="$TAB" read -r NAME CUT KEY PNG GOLDEN_STATUS; do
        [ -n "$NAME" ] || continue
        REFERENCE="$UNITY_DIR/$NAME.png"
        VALUES="-$TAB-$TAB-$TAB-$TAB-$TAB-$TAB-$TAB-$TAB-$TAB-"
        if [ ! -f "$REFERENCE" ]; then
            UNITY=missing
        else
            OVERLAY="$UNITY_OUT/$NAME.edges.png"
            rm -f "$OVERLAY"
            REPORT=$("$TOOL" --edges "$PNG" "$REFERENCE" --overlay "$OVERLAY" 2>&1)
            CODE=$?
            ROW=$(printf '%s\n' "$REPORT" | grep "^row$TAB" | head -n 1)
            if [ "$CODE" = 0 ] && [ -n "$ROW" ]; then
                UNITY=ok
                VALUES=$(printf '%s' "$ROW" | cut -f2-)
            else
                UNITY=$(printf '%s\n' "$REPORT" | sed -n 's/^error: *//p' | head -n 1 | tr -s ' \r' ' ')
                [ -n "$UNITY" ] || UNITY="failed ($CODE)"
            fi
        fi
        printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$NAME" "$CUT" "$KEY" "$GOLDEN_STATUS" "$UNITY" "$VALUES" >>"$SUMMARY"
    done

    ENGINE_HEAD=$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null)
    python3 - "$SUMMARY" "$UNITY_DIR" "$ENGINE_HEAD" "$ROOT" "$HEAD" "$CLEAN" <<'PYEOF'
import csv, html, pathlib, sys, urllib.parse

summary, unity_dir, engine_head, root, head, clean = sys.argv[1:7]
rows = list(csv.DictReader(open(summary, encoding='utf-8', newline=''), delimiter='\t'))
out = pathlib.Path(summary).parent
e = html.escape
parts = ["""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>StageEditor against Unity</title>
<style>
body { font: 14px system-ui, sans-serif; margin: 16px; background: #111; color: #ddd; }
code { color: #fff; }
a { color: #8cf; }
table { border-collapse: collapse; margin: 8px 0 24px; }
th, td { padding: 2px 8px; border-bottom: 1px solid #333; text-align: right; white-space: nowrap; }
th:first-child, td:first-child { text-align: left; }
.pictures { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin: 4px 0 20px; }
.pictures img { width: 100%; display: block; }
.blink { position: relative; display: block; }
.blink img.unity { position: absolute; inset: 0; opacity: 0; }
.blink:hover img.unity { animation: blink 1s steps(1) infinite; }
@keyframes blink { 50% { opacity: 1; } }
.legend span { padding: 0 6px; }
</style>
</head>
<body>
<h1>StageEditor against Unity</h1>
"""]
parts.append('<p>Engine <code>%s</code>; Backlot <code>%s</code> at <code>%s</code>, %s; Unity run <code>%s</code>; the tool\'s edge defaults (radius 2, density 5%%, lenience 2).</p>\n'
             % (e(engine_head), e(root), e(head), e(clean), e(unity_dir)))
parts.append("""<p>Report only: the style differs by design (sky gradient, bloom, tone mapping, SMAA), so these numbers point at placement and gate nothing. A Unity run must postdate the scene file.</p>
<p class="legend">Overlay: <span style="color:#a0a0a0">grey, edges both pictures have</span><span style="color:#f0f">magenta, the engine's alone</span><span style="color:#0f0">green, Unity's alone</span><span style="color:#ffd200">yellow, the worst tile</span>. Hover the engine's picture to blink it against Unity's.</p>
<table>
<tr><th>picture</th><th>golden</th><th>unity</th><th>engine near unity</th><th>unity near engine</th><th>worst tile</th><th>its share</th><th>shift</th><th>coincide</th><th>at (0,0)</th></tr>
""")
for r in rows:
    parts.append('<tr><td><a href="#%s">%s</a></td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n'
                 % (e(r['name']), e(r['name']), r['golden'], e(r['unity']), r['engine_near_unity'], r['unity_near_engine'],
                    r['worst_tile'], r['worst_tile_near'], r['shift'], r['shift_coincide'], r['zero_coincide']))
parts.append('</table>\n')
for cut in dict.fromkeys(r['cut'] for r in rows):
    parts.append('<h2>%s</h2>\n' % e(cut))
    for r in (r for r in rows if r['cut'] == cut):
        name = r['name']
        quoted = urllib.parse.quote(name)
        engine = '../%s.png' % quoted
        unity = pathlib.Path(unity_dir, name + '.png').resolve().as_uri()
        overlay = '%s.edges.png' % quoted
        parts.append('<h3 id="%s">%s: %s, engine near unity %s, unity near engine %s, shift %s</h3>\n'
                     % (e(name), e(name), e(r['unity']), r['engine_near_unity'], r['unity_near_engine'], r['shift']))
        parts.append('<div class="pictures">')
        parts.append('<a class="blink" href="%s"><img src="%s" alt="engine" loading="lazy"><img class="unity" src="%s" alt="" loading="lazy"></a>' % (engine, engine, unity))
        parts.append('<a href="%s"><img src="%s" alt="Unity" loading="lazy"></a>' % (unity, unity))
        if r['unity'] == 'ok':
            parts.append('<a href="%s"><img src="%s" alt="overlay" loading="lazy"></a>' % (overlay, overlay))
        parts.append('</div>\n')
parts.append('</body>\n</html>\n')
(out / 'report.html').write_text(''.join(parts), encoding='utf-8')

print()
print('unity: %s (report only)' % unity_dir)
line = '%-22s %-10s %-8s %7s %7s  %-6s %7s  %-6s %7s %7s'
print(line % ('picture', 'golden', 'unity', 'e->u', 'u->e', 'tile', 'share', 'shift', 'coin', 'at 0'))
for r in rows:
    print(line % (r['name'], r['golden'], r['unity'], r['engine_near_unity'], r['unity_near_engine'],
                  r['worst_tile'], r['worst_tile_near'], r['shift'], r['shift_coincide'], r['zero_coincide']))
print('unity report: %s' % (out / 'report.html'))
PYEOF
fi
exit "$STATUS"
