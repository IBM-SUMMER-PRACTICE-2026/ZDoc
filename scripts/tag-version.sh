#!/bin/sh
# tag-version.sh — cut a release tag from the in-source version.
#
# The version is the single source of truth: ZD_VERSION in zdoc/cli/options.h.
# A release is "due" only when that version has no matching `v<version>` tag
# yet, so this script is idempotent — running it on every push to main is a
# no-op until someone bumps ZD_VERSION.
#
# Outputs (for CI to consume via $GITHUB_OUTPUT, and to stdout):
#   tag=v<version>
#   created=true|false   (false when the tag already existed)
set -eu

REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION_FILE="$REPO_ROOT/zdoc/cli/options.h"

# Pull the string literal out of:  #define ZD_VERSION "0.1.0"
version=$(sed -n 's/.*#define[[:space:]]\{1,\}ZD_VERSION[[:space:]]\{1,\}"\([^"]*\)".*/\1/p' "$VERSION_FILE" | head -n1)
if [ -z "${version:-}" ]; then
    echo "tag-version: could not read ZD_VERSION from $VERSION_FILE" >&2
    exit 1
fi

tag="v$version"
created=false

if git rev-parse -q --verify "refs/tags/$tag" >/dev/null 2>&1; then
    echo "tag-version: $tag already exists — nothing to release." >&2
else
    git tag -a "$tag" -m "Release $tag"
    created=true
    echo "tag-version: created $tag at $(git rev-parse --short HEAD)" >&2
fi

# Machine-readable outputs.
if [ -n "${GITHUB_OUTPUT:-}" ]; then
    {
        echo "tag=$tag"
        echo "created=$created"
    } >>"$GITHUB_OUTPUT"
fi
echo "tag=$tag"
echo "created=$created"
