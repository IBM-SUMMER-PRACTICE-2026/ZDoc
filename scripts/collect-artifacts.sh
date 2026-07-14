#!/bin/sh
# collect-artifacts.sh — gather the build outputs a release should ship.
#
# Deliberately generic: it packages whatever the current tree actually
# produces, so it keeps working as components land and does not assume any
# particular feature is present. Today that means the built executables and,
# once the pipeline emits them, generated docs under zdoc-out/.
#
# Usage: collect-artifacts.sh [dist-dir]   (default: ./dist)
set -eu

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DIST=${1:-"$REPO_ROOT/dist"}

rm -rf "$DIST"
mkdir -p "$DIST/bin"

# Executables: the zdoc CLI and every per-component `zdoc-<module>` binary.
# Find by name + executable bit rather than a hardcoded list.
found_bin=0
for pat in zdoc 'zdoc-*'; do
    find "$REPO_ROOT" -type f -perm -u+x -name "$pat" \
        ! -path '*/.git/*' ! -name '*.o' ! -name '*.dSYM' 2>/dev/null |
    while IFS= read -r bin; do
        cp -p "$bin" "$DIST/bin/"
    done
    # count in the parent shell (subshell above can't set found_bin)
done
found_bin=$(find "$DIST/bin" -type f | wc -l | tr -d ' ')

# Generated documentation, if the run produced any (spec default out dir).
if [ -d "$REPO_ROOT/zdoc-out" ]; then
    cp -R "$REPO_ROOT/zdoc-out" "$DIST/docs"
fi

# Release manifest.
version=$(sed -n 's/.*#define[[:space:]]\{1,\}ZD_VERSION[[:space:]]\{1,\}"\([^"]*\)".*/\1/p' \
    "$REPO_ROOT/zdoc/cli/options.h" | head -n1)
{
    echo "version: ${version:-unknown}"
    echo "commit:  $(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "date:    $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "binaries: $found_bin"
} >"$DIST/MANIFEST.txt"

echo "collect-artifacts: wrote $found_bin binary(ies) to $DIST" >&2
cat "$DIST/MANIFEST.txt" >&2
