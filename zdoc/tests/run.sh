#!/usr/bin/env sh
# Golden-file tests for the zdoc CLI dry-run output.
#
#   sh run.sh            # check: run every case, diff against expected/*.txt
#   sh run.sh update     # regenerate expected/*.txt from the current binary
#
# Each cases/<name>.args holds the CLI flags for one run; the fixture source
# tree "src" is appended automatically. Output is normalised (CR stripped, then
# sorted) because fs_walk yields files in OS directory order, which is not
# stable across machines. Override the binary with ZDOC=/path/to/zdoc.
set -eu
# Globbing stays ON for the case loop; run_case disables it only while a case's
# args (e.g. **/tests/**) are word-split, then restores it.

HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE"

# Resolve the binary to an absolute path (cases run from the fixture dir).
if [ -z "${ZDOC:-}" ]; then
    if [ -x "$HERE/../zdoc.exe" ]; then ZDOC="$HERE/../zdoc.exe"; else ZDOC="$HERE/../zdoc"; fi
fi
case "$ZDOC" in /*) ;; *) ZDOC="$HERE/$ZDOC" ;; esac

MODE=${1:-check}
rc=0

# Build a throwaway fixture tree. Contents are fixed so output is deterministic.
FIX=$(mktemp -d)
trap 'rm -rf "$FIX"' EXIT
mkdir -p "$FIX/src/sub" "$FIX/src/tests"
: > "$FIX/src/a.c"
: > "$FIX/src/b.java"
: > "$FIX/src/sub/c.plx"
: > "$FIX/src/tests/skip.c"

# Run one case from inside the fixture dir; emit normalised output.
run_case() {
    set -f  # no globbing of args like **/tests/**
    out=$( cd "$FIX" && "$ZDOC" $(cat "$HERE/cases/$1.args") src 2>&1 | tr -d '\r' | sort )
    set +f
    printf '%s\n' "$out"
}

for args in cases/*.args; do
    name=$(basename "$args" .args)
    got=$(run_case "$name")
    exp="expected/$name.txt"
    if [ "$MODE" = update ]; then
        mkdir -p expected
        printf '%s\n' "$got" > "$exp"
        echo "updated $name"
        continue
    fi
    if [ -f "$exp" ] && printf '%s\n' "$got" | diff -u "$exp" - > "$FIX/diff" 2>&1; then
        echo "ok:   $name"
    else
        echo "FAIL: $name"; cat "$FIX/diff" 2>/dev/null || true; rc=1
    fi
done

# Behaviour checks that don't fit golden files: exit codes and config override.
if [ "$MODE" != update ]; then
    check_exit() { # desc expect flags...
        desc=$1; expect=$2; shift 2
        got=0
        ( cd "$FIX" && "$ZDOC" "$@" >/dev/null 2>&1 ) || got=$?
        if [ "$got" -eq "$expect" ]; then echo "ok:   $desc (exit $got)";
        else echo "FAIL: $desc exit $got != $expect"; rc=1; fi
    }
    check_exit "bad --mode"   2 --mode bogus src
    check_exit "unknown flag" 2 --nope src
    check_exit "no source"    2 --lang c
    check_exit "version"      0 --version
    check_exit "help"         0 --help

    # zdoc.yaml sets md; --output-format html must win.
    printf 'output_format: md\nrecursive: true\n' > "$FIX/zdoc.yaml"
    line=$( cd "$FIX" && "$ZDOC" --output-format html src 2>&1 | grep 'output-format:' || true )
    case "$line" in
        *html*) echo "ok:   config precedence (flag over yaml)" ;;
        *) echo "FAIL: config precedence: '$line'"; rc=1 ;;
    esac
    rm -f "$FIX/zdoc.yaml"

    [ "$rc" -eq 0 ] && echo "ALL PASS" || echo "SOME FAILED"
fi

exit $rc
