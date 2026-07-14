#!/bin/sh
# publish.sh — push the collected artifacts to a separate GitOps repo.
#
# GitOps flow: the release output lives in its own repo (and its own branch),
# separate from the source. This script ensures that repo exists (creating it
# via the GitHub API with curl if it does not), then replaces the branch
# contents with ./dist and pushes a commit named after the release tag.
#
# Environment:
#   RELEASE_REPO    owner/name of the target repo         (required)
#   RELEASE_TOKEN   token with repo scope for that owner  (required)
#   RELEASE_TAG     tag/label for the commit message      (default: manual)
#   GITOPS_BRANCH   branch to publish onto                (default: release)
#   DIST_DIR        artifacts to publish                  (default: ./dist)
#   API_URL         GitHub API base                       (default: api.github.com)
#   DRY_RUN=1       do everything except create/push (for local testing)
set -eu

: "${RELEASE_REPO:?set RELEASE_REPO=owner/name}"
DRY_RUN=${DRY_RUN:-0}
if [ "$DRY_RUN" != "1" ]; then
    : "${RELEASE_TOKEN:?set RELEASE_TOKEN (repo-scoped token)}"
fi
RELEASE_TAG=${RELEASE_TAG:-manual}
GITOPS_BRANCH=${GITOPS_BRANCH:-release}
API_URL=${API_URL:-https://api.github.com}
REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DIST_DIR=${DIST_DIR:-"$REPO_ROOT/dist"}

owner=${RELEASE_REPO%%/*}
name=${RELEASE_REPO#*/}

if [ ! -d "$DIST_DIR" ]; then
    echo "publish: $DIST_DIR not found — run 'make dist' first." >&2
    exit 1
fi

run() {
    if [ "$DRY_RUN" = "1" ]; then
        echo "DRY_RUN: $*" >&2
    else
        "$@"
    fi
}

# 1. Ensure the target repo exists (create under the owner if it 404s).
ensure_repo() {
    code=$(curl -sS -o /dev/null -w '%{http_code}' \
        -H "Authorization: Bearer $RELEASE_TOKEN" \
        -H "Accept: application/vnd.github+json" \
        "$API_URL/repos/$RELEASE_REPO")
    if [ "$code" = "200" ]; then
        echo "publish: repo $RELEASE_REPO exists." >&2
        return
    fi
    if [ "$code" != "404" ]; then
        echo "publish: unexpected status $code querying $RELEASE_REPO" >&2
        exit 1
    fi
    echo "publish: repo $RELEASE_REPO missing — creating it." >&2
    # Try org endpoint first; fall back to the authenticated user's repos.
    if ! curl -sS -f -X POST \
            -H "Authorization: Bearer $RELEASE_TOKEN" \
            -H "Accept: application/vnd.github+json" \
            "$API_URL/orgs/$owner/repos" \
            -d "{\"name\":\"$name\",\"private\":true,\"auto_init\":false}" >/dev/null 2>&1; then
        curl -sS -f -X POST \
            -H "Authorization: Bearer $RELEASE_TOKEN" \
            -H "Accept: application/vnd.github+json" \
            "$API_URL/user/repos" \
            -d "{\"name\":\"$name\",\"private\":true,\"auto_init\":false}" >/dev/null
    fi
}

if [ "$DRY_RUN" = "1" ]; then
    echo "DRY_RUN: would ensure repo $RELEASE_REPO exists via $API_URL" >&2
else
    ensure_repo
fi

# 2. Stage artifacts in a scratch git repo and push to the gitops branch.
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp -R "$DIST_DIR/." "$work/"

git -C "$work" init -q
git -C "$work" checkout -q -b "$GITOPS_BRANCH"
git -C "$work" add -A
git -C "$work" -c user.name="zdoc-ci" -c user.email="ci@zdoc.local" \
    commit -q -m "Release $RELEASE_TAG"

remote="https://x-access-token:${RELEASE_TOKEN:-TOKEN}@github.com/$RELEASE_REPO.git"
git -C "$work" remote add origin "$remote"

# Replace the branch wholesale — the gitops branch mirrors the latest release.
run git -C "$work" push -f origin "$GITOPS_BRANCH"

echo "publish: pushed $RELEASE_TAG to $RELEASE_REPO ($GITOPS_BRANCH)." >&2
