#!/bin/sh
# Fails when a C++ file under a directory listed in Tools/formatted.txt
# differs from what clang-format makes of it.
#
# usage: Tools/format_check.sh
#
# Same contract as Tools/format_check.ps1: one directory per line, relative to
# the repository root; blank lines and lines starting with # are skipped.
# Every .cpp, .hpp and .h below a listed directory is checked with
# clang-format --dry-run -Werror. An empty list passes; a listed directory
# that does not exist fails.
set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LIST="$REPO_ROOT/Tools/formatted.txt"

DIRECTORIES=""
while IFS= read -r LINE || [ -n "$LINE" ]; do
    LINE="$(printf '%s' "$LINE" | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    case "$LINE" in
    "" | "#"*) continue ;;
    esac
    if [ ! -d "$REPO_ROOT/$LINE" ]; then
        echo "FAIL: $LINE is listed but is not a directory" >&2
        exit 1
    fi
    DIRECTORIES="$DIRECTORIES $LINE"
done <"$LIST"

if [ -z "$DIRECTORIES" ]; then
    echo "format_check: no directory listed in Tools/formatted.txt"
    exit 0
fi

cd "$REPO_ROOT" || exit 1
COUNT=0
STATUS=0
OUTPUT=""
for DIRECTORY in $DIRECTORIES; do
    FOUND="$(find "$DIRECTORY" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) | wc -l)"
    COUNT=$((COUNT + FOUND))
    RESULT="$(find "$DIRECTORY" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) \
        -exec clang-format --dry-run -Werror {} + 2>&1)" || STATUS=1
    OUTPUT="$OUTPUT
$RESULT"
done

if [ "$STATUS" -ne 0 ]; then
    # one line per file, not one per violation
    DRIFTED="$(printf '%s\n' "$OUTPUT" | sed -n 's/^\(.*\):[0-9]*:[0-9]*: error:.*/\1/p' | sort -u)"
    if [ -z "$DRIFTED" ]; then
        printf '%s\n' "$OUTPUT" | tail -n 20 >&2
        echo "FAIL: clang-format did not run cleanly" >&2
        exit 1
    fi
    printf '%s\n' "$DRIFTED" >&2
    echo "FAIL: $(printf '%s\n' "$DRIFTED" | wc -l | tr -d ' ') of $COUNT files differ; run clang-format -i on them" >&2
    exit 1
fi
echo "format_check: $COUNT files in$DIRECTORIES match clang-format"
exit 0
