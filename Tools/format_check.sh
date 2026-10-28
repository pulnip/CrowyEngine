#!/bin/sh
# Fails when a C++ file under Engine/ differs from what clang-format makes
# of it.
#
# usage: Tools/format_check.sh
#
# Same contract as Tools/format_check.ps1: every .cpp, .hpp and .h below
# Engine/ is checked with clang-format --dry-run -Werror.
set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT" || exit 1

COUNT="$(find Engine -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) | wc -l | tr -d ' ')"
if [ "$COUNT" -eq 0 ]; then
    echo "FAIL: no C++ file under Engine" >&2
    exit 1
fi

STATUS=0
OUTPUT="$(find Engine -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) \
    -exec clang-format --dry-run -Werror {} + 2>&1)" || STATUS=1

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
echo "format_check: $COUNT files in Engine match clang-format"
exit 0
