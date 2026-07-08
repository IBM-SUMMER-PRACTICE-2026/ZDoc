#!/bin/sh
# Renderer regression tests: runs zdoc-html-renderer against the committed
# model JSONs in data/ (plus ../sample.json) and asserts on exit codes and
# output contents. POSIX sh - works under Git Bash on Windows and on Linux.
#
#   sh renderer/html_renderer/tests/run_tests.sh
#
# Build the renderer first (see ../Makefile). Exits 0 when every check
# passes; on failure the rendered pages are left in tests/tmp/ to inspect.
cd "$(dirname "$0")" || exit 1

EXE=../zdoc-html-renderer
[ -x "$EXE" ] || EXE=../zdoc-html-renderer.exe
[ -x "$EXE" ] || { echo "zdoc-html-renderer not built - run make in renderer/html_renderer first"; exit 1; }

TMP=tmp
rm -rf "$TMP"
mkdir -p "$TMP"

fails=0
check() { # check <description> <status: 0 = pass>
    if [ "$2" -eq 0 ]; then
        echo "ok   $1"
    else
        echo "FAIL $1"
        fails=$((fails + 1))
    fi
}

# --- sample.json: the full contract example, including diagram + refs ------
"$EXE" --out-dir "$TMP/full" ../sample.json
check "sample.json renders (exit 0)" $?
OUT=$TMP/full/index.html
grep -q 'id="sym-doStuff"' "$OUT";            check "symbol anchor emitted" $?
grep -q '<pre class="mermaid">' "$OUT";       check "block diagram emitted" $?
grep -q 'href="#sym-bareMethod"' "$OUT";      check "cross-reference link emitted" $?
grep -q 'Block diagram omitted' "$OUT";       check "no-JS diagram note emitted" $?
grep -q '<script' "$OUT";                     check "diagram/reveal scripts emitted" $?

# --- offline.json: no diagrams/refs, so the page must be script-free -------
"$EXE" --out-dir "$TMP/offline" data/offline.json
check "offline.json renders (exit 0)" $?
OUT=$TMP/offline/index.html
if grep -q '<script' "$OUT"; then st=1; else st=0; fi
check "offline output has no scripts" $st
grep -q 'alphaInit' "$OUT";                   check "documented symbol emitted" $?
grep -q 'No documented symbols' "$OUT";       check "empty-file note emitted" $?

# --- parser_error.json: extractor flagged the file's parser as failed ------
"$EXE" --out-dir "$TMP/error" data/parser_error.json
check "parser_error.json renders (exit 0)" $?
grep -q 'Parser failed for this file' "$TMP/error/index.html"
check "parser-failed note emitted" $?

# --- stdin input ------------------------------------------------------------
"$EXE" --out-dir "$TMP/stdin" < data/offline.json
check "reads model from stdin (exit 0)" $?

# --- nested --out-dir is created like mkdir -p ------------------------------
"$EXE" --out-dir "$TMP/deep/a/b" data/offline.json
check "nested out-dir created (exit 0)" $?
[ -f "$TMP/deep/a/b/index.html" ];            check "index.html at nested path" $?

# Windows also accepts backslash separators (mkdir_p splits on both there)
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*)
    "$EXE" --out-dir "$TMP\\bs\\deep" data/offline.json
    check "backslash out-dir accepted (exit 0)" $?
    [ -f "$TMP/bs/deep/index.html" ];         check "index.html at backslash path" $?
    ;;
esac

# --- failure exit codes -----------------------------------------------------
"$EXE" --out-dir "$TMP/bad" data/malformed.json 2>/dev/null
[ $? -eq 1 ];                                 check "malformed JSON exits 1" $?
[ ! -e "$TMP/bad/index.html" ];               check "no output for malformed JSON" $?

"$EXE" --bogus-option 2>/dev/null
[ $? -eq 2 ];                                 check "unknown option exits 2" $?

echo
if [ "$fails" -eq 0 ]; then
    echo "all tests passed"
    rm -rf "$TMP"
    exit 0
else
    echo "$fails check(s) failed - output kept in tests/$TMP"
    exit 1
fi
