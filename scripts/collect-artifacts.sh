#!/bin/sh
# collect-artifacts.sh — gather the build outputs a release should ship.
#
# The release ships a single binary: the zdoc CLI (built as a self-contained
# executable that runs the whole pipeline). The per-component zdoc-<module>
# build outputs are development/test tools and are deliberately NOT shipped.
#
# For multi-platform releases, set PLATFORM (e.g. linux-x64/macos-arm64/...) and
# the output is nested under dist/<platform>/ so per-OS binaries never collide
# when every platform's artifacts are combined. Unset PLATFORM keeps a flat
# dist/ for local `make dist`.
#
# Usage: collect-artifacts.sh [dist-dir]   (default: ./dist)
#   env: PLATFORM  optional platform label
#        CLI_DIR   dir holding the shipped binary (default: zdoc/cli)
set -eu

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DIST=${1:-"$REPO_ROOT/dist"}
PLATFORM=${PLATFORM:-}

if [ -n "$PLATFORM" ]; then
    DEST="$DIST/$PLATFORM"
else
    DEST="$DIST"
fi

rm -rf "$DEST"
mkdir -p "$DEST/bin"

# Take only the CLI binary, by path, so it is never confused with the
# same-named daemon binary in zdoc/ (both are called `zdoc`). Matches with or
# without a Windows .exe suffix.
CLI_DIR=${CLI_DIR:-zdoc/cli}
find "$REPO_ROOT/$CLI_DIR" -maxdepth 1 -type f -perm -u+x \
    \( -name 'zdoc' -o -name 'zdoc.exe' \) \
    -exec cp -p {} "$DEST/bin/" \;

if [ -z "$(ls -A "$DEST/bin" 2>/dev/null)" ]; then
    echo "collect-artifacts: no CLI binary found in $CLI_DIR — did 'make all' run?" >&2
    exit 1
fi

# Ensure Windows binaries carry a .exe suffix. Native MSYS2 builds already do;
# this is a harmless no-op there and a safety net for any that don't.
case "$PLATFORM" in
    windows*)
        for f in "$DEST"/bin/*; do
            [ -f "$f" ] || continue
            case "$f" in
                *.exe) ;;
                *) mv "$f" "$f.exe" ;;
            esac
        done
        ;;
esac

found_bin=$(find "$DEST/bin" -type f | wc -l | tr -d ' ')

# Generated documentation, if the run produced any (spec default out dir).
if [ -d "$REPO_ROOT/zdoc-out" ]; then
    cp -R "$REPO_ROOT/zdoc-out" "$DEST/docs"
fi

# Release manifest.
version=$(sed -n 's/.*#define[[:space:]]\{1,\}ZD_VERSION[[:space:]]\{1,\}"\([^"]*\)".*/\1/p' \
    "$REPO_ROOT/zdoc/cli/options.h" | head -n1)
{
    echo "version:  ${version:-unknown}"
    echo "platform: ${PLATFORM:-native}"
    echo "commit:   $(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "date:     $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "binaries: $found_bin"
} >"$DEST/MANIFEST.txt"

echo "collect-artifacts: wrote $found_bin binary(ies) to $DEST" >&2
cat "$DEST/MANIFEST.txt" >&2
