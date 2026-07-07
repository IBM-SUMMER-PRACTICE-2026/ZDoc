#!/bin/sh
# Process-level tests for zdoc-bob-client: stub providers exercise the
# repair / retry / fallback / timeout / cache paths end to end.
set -e
cd "$(dirname "$0")/.."

BIN=./zdoc-bob-client
FIX=tests/fixtures/input.json
STUBS="$(pwd)/tests/stubs"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

chmod +x "$STUBS"/bob-*

fails=0
fail() { echo "FAIL: $1"; fails=$((fails + 1)); }
ok() { echo "  ok: $1"; }

pycheck() { # pycheck <json-file> <python-expr-that-must-be-True> <desc>
    if python3 -c "
import json,sys
d=json.load(open('$1'))
mods=d['modules']
syms={s['name']:s for m in mods for s in m['symbols']}
sys.exit(0 if ($2) else 1)
"; then ok "$3"; else fail "$3"; fi
}

echo "== happy path (bob-good) =="
: > "$WORK/count"
ZDOC_STUB_COUNT="$WORK/count" $BIN --input "$FIX" \
    --ai-command "$STUBS/bob-good explain --lang {lang}" \
    --ai-no-cache --fail-log "$WORK/fail.log" > "$WORK/out1.json" 2> "$WORK/err1"
pycheck "$WORK/out1.json" \
    "syms['remove_key']['block_diagram'].startswith('flowchart TD')" \
    "diagram attached and rendered as mermaid"
pycheck "$WORK/out1.json" \
    "'audit_removal' in syms['remove_key'].get('call_edges',[])" \
    "call edge harvested and matched to extracted symbol"
pycheck "$WORK/out1.json" \
    "'block_diagram' in syms['count_keys'] and 'block_diagram' in syms['noop']" \
    "all three functions processed"
pycheck "$WORK/out1.json" \
    "'block_diagram' not in syms['audit_removal']" \
    "prototypes not diagrammed"
[ "$(wc -l < "$WORK/count" | tr -d ' ')" = "3" ] \
    && ok "exactly one provider call per function" \
    || fail "expected 3 provider calls, got $(wc -l < "$WORK/count")"
[ -f "$WORK/fail.log" ] && fail "no failure log expected" || ok "no failure log"

echo "== repair path (bob-noisy) =="
$BIN --input "$FIX" --ai-command "$STUBS/bob-noisy x --lang {lang}" \
    --ai-no-cache --fail-log "$WORK/fail2.log" > "$WORK/out2.json" 2> "$WORK/err2"
pycheck "$WORK/out2.json" \
    "syms['remove_key']['block_diagram'].startswith('flowchart TD')" \
    "noisy response repaired, not rejected"
grep -q "repaired 3" "$WORK/err2" && ok "stats count repairs" \
    || fail "stats should count 3 repairs: $(cat "$WORK/err2")"

echo "== retry path (bob-flaky) =="
: > "$WORK/count3"
ZDOC_STUB_COUNT="$WORK/count3" $BIN --input "$FIX" \
    --ai-command "$STUBS/bob-flaky x --lang {lang}" \
    --ai-jobs 1 --ai-no-cache --fail-log "$WORK/fail3.log" \
    > "$WORK/out3.json" 2> "$WORK/err3"
pycheck "$WORK/out3.json" \
    "all('block_diagram' in syms[k] for k in ('remove_key','count_keys','noop'))" \
    "flaky provider succeeds via retry"
[ "$(wc -l < "$WORK/count3" | tr -d ' ')" = "6" ] \
    && ok "two invocations per symbol (retry once)" \
    || fail "expected 6 invocations, got $(wc -l < "$WORK/count3")"

echo "== fallback path (bob-garbage) =="
: > "$WORK/count4"
ZDOC_STUB_COUNT="$WORK/count4" $BIN --input "$FIX" \
    --ai-command "$STUBS/bob-garbage x --lang {lang}" \
    --ai-no-cache --fail-log "$WORK/fail4.log" > "$WORK/out4.json" 2> "$WORK/err4" \
    || true
pycheck "$WORK/out4.json" \
    "syms['remove_key'].get('diagram_error') is True and 'block_diagram' not in syms['remove_key']" \
    "diagram_error set after both attempts fail"
[ -s "$WORK/fail4.log" ] && ok "failures logged" || fail "failure log missing"
grep -q "raw response" "$WORK/fail4.log" && ok "raw response captured" \
    || fail "raw response not in failure log"
[ "$(wc -l < "$WORK/count4" | tr -d ' ')" = "6" ] \
    && ok "retried once per symbol before giving up" \
    || fail "expected 6 invocations, got $(wc -l < "$WORK/count4")"

echo "== nonzero exit (bob-exit1) =="
$BIN --input "$FIX" --ai-command "$STUBS/bob-exit1 x --lang {lang}" \
    --ai-no-cache --fail-log "$WORK/fail5.log" > "$WORK/out5.json" 2> /dev/null || true
pycheck "$WORK/out5.json" \
    "syms['remove_key'].get('diagram_error') is True" \
    "nonzero exit takes the fallback path"

echo "== timeout (bob-slow) =="
start=$(date +%s)
$BIN --input "$FIX" --ai-command "$STUBS/bob-slow x --lang {lang}" \
    --ai-timeout 1 --ai-jobs 3 --ai-no-cache --fail-log "$WORK/fail6.log" \
    > "$WORK/out6.json" 2> /dev/null || true
elapsed=$(( $(date +%s) - start ))
pycheck "$WORK/out6.json" \
    "syms['remove_key'].get('diagram_error') is True" \
    "timeout takes the fallback path"
[ "$elapsed" -lt 15 ] && ok "slow provider killed (${elapsed}s)" \
    || fail "timeout did not kill provider (${elapsed}s)"

echo "== cache (two runs, one set of provider calls) =="
: > "$WORK/count7"
ZDOC_STUB_COUNT="$WORK/count7" $BIN --input "$FIX" \
    --ai-command "$STUBS/bob-good explain --lang {lang}" \
    --ai-cache-dir "$WORK/cache" --fail-log "$WORK/fail7.log" \
    > "$WORK/out7a.json" 2> /dev/null
ZDOC_STUB_COUNT="$WORK/count7" $BIN --input "$FIX" \
    --ai-command "$STUBS/bob-good explain --lang {lang}" \
    --ai-cache-dir "$WORK/cache" --fail-log "$WORK/fail7.log" \
    > "$WORK/out7b.json" 2> "$WORK/err7b"
[ "$(wc -l < "$WORK/count7" | tr -d ' ')" = "3" ] \
    && ok "second run served entirely from cache" \
    || fail "expected 3 total calls across two runs, got $(wc -l < "$WORK/count7")"
grep -q "cached 3" "$WORK/err7b" && ok "stats report cache hits" \
    || fail "stats should report cached 3: $(cat "$WORK/err7b")"
cmp -s "$WORK/out7a.json" "$WORK/out7b.json" \
    && ok "cached output byte-identical" || fail "cache changed the output"

echo "== record mode =="
$BIN --input "$FIX" --ai-command "$STUBS/bob-good explain --lang {lang}" \
    --ai-no-cache --ai-record "$WORK/rec" --fail-log "$WORK/fail8.log" \
    > /dev/null 2>&1
reqs=$(ls "$WORK/rec"/*.req.txt 2>/dev/null | wc -l | tr -d ' ')
[ "$reqs" = "3" ] && ok "request/response pairs recorded" \
    || fail "expected 3 recorded requests, got $reqs"
grep -q "FUNCTION (C):" "$WORK/rec"/*.req.txt && ok "snippets in records" \
    || fail "recorded requests lack the snippet format"

echo "== missing --ai-context body =="
printf '%s' '{"modules":[{"file":"x.c","language":"c","symbols":[{"kind":"function","name":"f","signature":"void f(void)"}]}]}' > "$WORK/nobody.json"
$BIN --input "$WORK/nobody.json" \
    --ai-command "$STUBS/bob-good explain --lang {lang}" \
    --ai-no-cache --fail-log "$WORK/fail9.log" > "$WORK/out9.json" 2> /dev/null || true
pycheck "$WORK/out9.json" \
    "syms['f'].get('diagram_error') is True" \
    "symbols without body marked, run continues"

echo "== end-to-end: zdoc-c-parser --ai-context | zdoc-bob-client =="
CPDIR=../../parser/c_parser
if make -C "$CPDIR" > /dev/null 2>&1 && [ -x "$CPDIR/zdoc-c-parser" ]; then
    "$CPDIR/zdoc-c-parser" --ai-context "$CPDIR/tests/sample.c" \
        > "$WORK/e2e_in.json"
    $BIN --input "$WORK/e2e_in.json" \
        --ai-command "$STUBS/bob-good explain --lang {lang}" \
        --ai-no-cache --fail-log "$WORK/faile2e.log" \
        > "$WORK/e2e_out.json" 2> /dev/null
    pycheck "$WORK/e2e_out.json" \
        "syms['widget_term']['block_diagram'].startswith('flowchart TD') and syms['widget_reset']['block_diagram'].startswith('flowchart TD')" \
        "real parser output flows through to diagrams"
    pycheck "$WORK/e2e_out.json" \
        "len(mods[0]['declarations']) >= 6 and 'body' in syms['widget_term']" \
        "ai-context declarations and bodies preserved in output"
    pycheck "$WORK/e2e_out.json" \
        "'block_diagram' not in syms['widget_init']" \
        "prototypes still skipped end to end"
else
    echo "  skip: zdoc-c-parser not buildable here"
fi

echo
if [ "$fails" -gt 0 ]; then
    echo "run_tests.sh: $fails FAILURE(S)"
    exit 1
fi
echo "run_tests.sh: all process tests passed"
