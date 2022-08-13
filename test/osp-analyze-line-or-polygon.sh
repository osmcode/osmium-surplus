#!/bin/bash
#-----------------------------------------------------------------------------
#
#  test/osp-analyze-line-or-polygon.sh SOURCE_DIR
#
#-----------------------------------------------------------------------------

set -euo pipefail

SRCDIR="$1"

mkdir -p osp-analyze-line-or-polygon

for input in "$SRCDIR"/test/osp-analyze-line-or-polygon/*.opl; do
    output="osp-analyze-line-or-polygon/"$(basename -s .opl "$input")
    ../src/osp-analyze-line-or-polygon -e "$SRCDIR/filter-patterns" "$input" >"$output.result"
    expected="$SRCDIR/test/$output.expected"
    echo diff -u "$expected" "$output.result"
done

#-----------------------------------------------------------------------------
