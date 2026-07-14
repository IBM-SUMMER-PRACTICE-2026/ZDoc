#!/bin/sh
# install.sh — fetch the right prebuilt zdoc CLI binary for this machine and
# put it on PATH as `zdoc`.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/<owner>/ZDoc/main/scripts/install.sh | sh
#
# Binaries come from the GitOps release repo that .github/workflows/release.yml
# publishes to on every version-tagged release - NOT from this source repo,
# which never carries built binaries. That repo's layout (one folder per
# version, holding one binary per platform) is exactly what this script reads:
#   <version>/zdoc-<platform>[.exe]
# where <platform> is one of: linux-x64, linux-arm64, macos-x64, macos-arm64,
# windows-x64, windows-arm64 - matching the platform matrix in release.yml.
#
# Environment:
#   ZDOC_RELEASE_REPO    owner/name of the GitOps release repo.
#                         Must match the RELEASE_REPO repository variable
#                         configured for .github/workflows/release.yml - the
#                         default below is this repo's; override it if
#                         you're running this against a fork.
#   ZDOC_RELEASE_BRANCH   branch the release repo publishes to (default: release)
#   ZDOC_VERSION          version to install, e.g. v0.2.0     (default: latest)
#   ZDOC_INSTALL_DIR      where to put the zdoc binary        (default: $HOME/bin)
set -eu

RELEASE_REPO=${ZDOC_RELEASE_REPO:-IBM-SUMMER-PRACTICE-2026/ZDoc-releases}
RELEASE_BRANCH=${ZDOC_RELEASE_BRANCH:-release}
INSTALL_DIR=${ZDOC_INSTALL_DIR:-"$HOME/bin"}
API_URL=${ZDOC_API_URL:-https://api.github.com}
RAW_URL=${ZDOC_RAW_URL:-https://raw.githubusercontent.com}

# 1. Detect platform - the four labels below must be kept in sync with the
#    platform matrix in .github/workflows/release.yml.
os=$(uname -s)
arch=$(uname -m)

case "$os" in
    Linux)                 os_tag=linux ;;
    Darwin)                os_tag=macos ;;
    MINGW*|MSYS*|CYGWIN*)  os_tag=windows ;;
    *)
        echo "install: unsupported OS '$os'" >&2
        exit 1
        ;;
esac

case "$arch" in
    x86_64|amd64)   arch_tag=x64 ;;
    aarch64|arm64)  arch_tag=arm64 ;;
    *)
        echo "install: unsupported architecture '$arch'" >&2
        exit 1
        ;;
esac

platform="$os_tag-$arch_tag"
ext=""
[ "$os_tag" = "windows" ] && ext=".exe"

# 2. Resolve which version to install - explicit ZDOC_VERSION, or whichever
#    v<major>.<minor>.<patch> folder at the release repo's root sorts
#    highest. Each folder name is turned into a zero-padded, lexicographically
#    sortable key so plain `sort` (no GNU -V extension needed) orders them
#    correctly regardless of digit width (v0.9.0 before v0.10.0).
version=${ZDOC_VERSION:-}
if [ -z "$version" ]; then
    echo "install: looking up the latest published version..." >&2
    versions=$(curl -fsSL -H "Accept: application/vnd.github+json" \
        "$API_URL/repos/$RELEASE_REPO/contents/?ref=$RELEASE_BRANCH" \
        | grep -o '"name": *"v[0-9][0-9.]*"' \
        | sed 's/.*"\(v[0-9][0-9.]*\)"/\1/')

    version=$(
        for v in $versions; do
            printf '%s' "$v" | sed 's/^v//' \
                | awk -F. -v ver="$v" '{ printf "%05d.%05d.%05d %s\n", $1, $2, $3, ver }'
        done | sort | tail -n1 | awk '{ print $2 }'
    )

    if [ -z "$version" ]; then
        echo "install: no published version found in $RELEASE_REPO ($RELEASE_BRANCH)" >&2
        echo "install: is ZDOC_RELEASE_REPO set correctly? (currently: $RELEASE_REPO)" >&2
        exit 1
    fi
fi

echo "install: fetching zdoc $version for $platform..." >&2

# 3. Download the binary for this platform/version straight from the raw
#    file - no auth needed for a public repo/branch.
url="$RAW_URL/$RELEASE_REPO/$RELEASE_BRANCH/$version/zdoc-$platform$ext"
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

if ! curl -fsSL -o "$tmp" "$url"; then
    echo "install: failed to download $url" >&2
    echo "install: does zdoc-$platform$ext exist under $version/ in $RELEASE_REPO?" >&2
    exit 1
fi

# 4. Install it on PATH as `zdoc` - same default directory as `make install`
#    in zdoc/cli/Makefile, so both installation paths agree on where it ends
#    up.
mkdir -p "$INSTALL_DIR"
dest="$INSTALL_DIR/zdoc$ext"
mv "$tmp" "$dest"
chmod +x "$dest"
trap - EXIT

echo "install: installed zdoc $version -> $dest" >&2

# 5. Persist INSTALL_DIR onto PATH so `zdoc` resolves without a manual step,
#    if it isn't already there.
case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;;
    *)
        if [ "$os_tag" = "windows" ] && command -v powershell.exe >/dev/null 2>&1; then
            # A POSIX shell's rc file only reaches other POSIX shells (this
            # one, or future Git Bash/MSYS sessions) - cmd.exe and PowerShell
            # read PATH from the registry instead, so that's what actually
            # needs updating for `zdoc` to resolve everywhere on Windows.
            win_dir=$(cygpath -w "$INSTALL_DIR" 2>/dev/null \
                || printf '%s' "$INSTALL_DIR" | sed -E 's#^/([a-zA-Z])/#\1:/#; s#/#\\#g')
            result=$(ZDOC_WIN_DIR="$win_dir" powershell.exe -NoProfile -Command '
                $d = $env:ZDOC_WIN_DIR
                $p = [Environment]::GetEnvironmentVariable("PATH","User")
                if (($p -split ";") -notcontains $d) {
                    $new = if ($p) { "$p;$d" } else { $d }
                    [Environment]::SetEnvironmentVariable("PATH", $new, "User")
                    Write-Output "added"
                } else {
                    Write-Output "unchanged"
                }
            ' 2>/dev/null)
            if [ "$result" = "added" ]; then
                echo "install: added $win_dir to your Windows user PATH - open a new cmd/PowerShell/Git Bash window for it to take effect." >&2
            else
                echo "install: could not update the Windows PATH automatically - add $win_dir to it yourself." >&2
            fi
        else
            # Pick the rc file the user's actual shell reads, not a generic
            # guess - zsh doesn't source ~/.profile, for instance.
            case "${SHELL:-}" in
                */zsh)  rc="$HOME/.zshrc" ;;
                */bash) rc="$HOME/.bashrc" ;;
                *)      rc="$HOME/.profile" ;;
            esac
            if grep -qF "$INSTALL_DIR" "$rc" 2>/dev/null; then
                echo "install: $INSTALL_DIR already referenced in $rc - open a new terminal for it to take effect." >&2
            else
                printf '\nexport PATH="%s:$PATH"\n' "$INSTALL_DIR" >> "$rc"
                echo "install: added $INSTALL_DIR to PATH in $rc - open a new terminal, or run: source $rc" >&2
            fi
        fi
        ;;
esac

"$dest" --version 2>/dev/null || true
