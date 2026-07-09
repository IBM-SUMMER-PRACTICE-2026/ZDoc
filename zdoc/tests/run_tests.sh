#!/bin/sh
# Smoke tests for the zdoc CLI. Exercises the interface only, so it
# passes without the daemon or any stage binaries.
set -e
cd "$(dirname "$0")/.."

BIN=./zdoc
[ -x ./zdoc.exe ] && BIN=./zdoc.exe

$BIN --version | grep -q "zdoc" || { echo "FAIL: --version"; exit 1; }
$BIN --help | grep -q -- "--output-format" || { echo "FAIL: --help"; exit 1; }
$BIN | grep -q "documentation generator" || { echo "FAIL: bare zdoc about text"; exit 1; }

if $BIN --definitely-not-an-option 2>/dev/null; then
    echo "FAIL: unknown option accepted"; exit 1
fi
if $BIN --recursive 2>/dev/null; then
    echo "FAIL: missing source path accepted"; exit 1
fi
if $BIN --mode sideways . 2>/dev/null; then
    echo "FAIL: bad --mode value accepted"; exit 1
fi
if $BIN ./no-such-dir-xyz 2>/dev/null; then
    echo "FAIL: nonexistent source path accepted"; exit 1
fi

$BIN --output-format html --title "T" . | grep -q '"output_format": "html"' \
    || { echo "FAIL: request JSON"; exit 1; }
$BIN --lang plx,c,java . | grep -q '"languages": \["plx", "c", "java"\]' \
    || { echo "FAIL: --lang comma list"; exit 1; }
$BIN --lang assembler,C++ . | grep -q '"languages": \["asm", "cpp"\]' \
    || { echo "FAIL: --lang alias canonicalization"; exit 1; }
if $BIN --lang klingon . 2>/dev/null; then
    echo "FAIL: unknown language accepted"; exit 1
fi

# zdoc.json config is honoured (no zdoc.yaml in the temp dir)
mkdir -p tests/tmp-json
printf '{\n  "output_format": "html",\n  "title": "FromJson",\n  "recursive": true,\n  "languages": ["plx", "c"]\n}\n' > tests/tmp-json/zdoc.json
( cd tests/tmp-json && "../../${BIN#./}" . ) | grep -q '"title": "FromJson"' \
    || { echo "FAIL: zdoc.json config"; exit 1; }
( cd tests/tmp-json && "../../${BIN#./}" . ) | grep -q '"languages": \["plx", "c"\]' \
    || { echo "FAIL: zdoc.json languages list"; exit 1; }
rm -rf tests/tmp-json

echo "zdoc CLI smoke tests passed"
