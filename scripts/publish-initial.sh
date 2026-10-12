#!/usr/bin/env bash
set -Eeuo pipefail
cd -- "$(dirname "$(dirname "$(realpath "$0")")")"
if [ "${1:-}" = '--public' ]; then VIS='--public'; else VIS='--private'; fi
command -v gh >/dev/null || { echo 'Install gh, then gh auth login' >&2;exit 1; }
ACCOUNT="$(gh api user --jq '.login')"
[ "${ACCOUNT,,}" = 'tomsellecksmustache' ] || { echo "Wrong gh account: $ACCOUNT" >&2;exit 1; }
python3 tools/nxctl.py audit
[ "$(git tag --list 'v0.0.1')" = 'v0.0.1' ] || { echo 'Bootstrap milestone missing' >&2;exit 1; }
if gh repo view TomSellecksMustache/Lufia-II-NX >/dev/null 2>&1; then
 echo 'Remote exists. Cannot safely create a second one.' >&2; exit 1
fi
gh repo create TomSellecksMustache/Lufia-II-NX "$VIS" --source=. --remote=origin --push
git push origin v0.0.1
gh release create v0.0.1 --repo TomSellecksMustache/Lufia-II-NX \
 --title 'v0.0.1 - ROM-free scaffold' --notes-file docs/RELEASE_NOTES.md --prerelease
