#!/bin/sh
# collect-artifacts.sh — gather the build outputs a release should ship.
#
# Deliberately generic: it packages whatever the current tree actually
# produces, so it keeps working as components land and does not assume any
# particular feature is present. Today that means the built executables and,
# once the pipeline emits them, generated docs under zdoc-out/.
#
# For multi-platform releases, set PLATFORM (e.g. linux/macos/windows) and the
# output is nested under dist/<platform>/ so per-OS binaries never collide when
# every platform's artifacts are combined. Unset PLATFORM keeps a flat dist/
# for local `make dist`.
#
# Usage: collect-artifacts.sh [dist-dir]   (default: ./dist)
#   env: PLATFORM  optional platform label
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

# Executables: the zdoc CLI and every per-component `zdoc-<module>` binary,
# with or without a Windows .exe suffix. Match by name + executable bit rather
# than a hardcoded list; skip VCS, build scratch, macOS .dSYM debug bundles and
# our own output dir.
find "$REPO_ROOT" -type f -perm -u+x \
    \( -name 'zdoc' -o -name 'zdoc.exe' -o -name 'zdoc-*' \) \
    ! -path '*/.git/*' ! -path '*.dSYM/*' ! -path "$DIST/*" \
    ! -name '*.o' ! -name '*.obj' \
    -exec cp -p {} "$DEST/bin/" \;

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
