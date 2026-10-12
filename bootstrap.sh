#!/usr/bin/env bash
# Run from extracted starter directory on Ubuntu.
set -Eeuo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="${HOME}/src/Lufia-II-NX"
ROM=''
GITHUB=0
PUBLIC=0
help() {
 cat <<'USAGE'
Usage: ./bootstrap.sh --rom /absolute/path/Lufia-II-USA.sfc [options]
Options:
  --dir PATH           New project folder (default: ~/src/Lufia-II-NX)
  --github             Create TomSellecksMustache/Lufia-II-NX and release v0.0.1
  --public             With --github, make repo PUBLIC (default is private)

No ROM bytes or generated game code are copied to the project directory.
Installs nothing system-wide; run scripts/install-toolchain.sh separately.
USAGE
}
while [ "$#" -gt 0 ]; do
 case "$1" in
  --rom) ROM="${2:?missing ROM path}"; shift 2;;
  --dir) PROJECT="${2:?missing path}"; shift 2;;
  --github) GITHUB=1; shift;;
  --public) PUBLIC=1; shift;;
  -h|--help) help; exit 0;;
  *) echo "Unknown argument: $1" >&2; help; exit 2;;
 esac
done
[ -n "$ROM" ] || { help; exit 2; }
ROM="$(realpath -- "$ROM")"
[ -f "$ROM" ] || { echo "ROM not found: $ROM" >&2; exit 1; }
PROJECT="$(realpath -m -- "$PROJECT")"
[ ! -e "$PROJECT" ] || { echo "Project path already exists, refusing to overwrite: $PROJECT" >&2; exit 1; }
case "$ROM" in "$PROJECT"/*) echo 'ROM must remain OUTSIDE repository' >&2; exit 1;; esac
command -v git >/dev/null || { echo 'sudo apt install git' >&2; exit 1; }
command -v python3 >/dev/null || { echo 'sudo apt install python3' >&2; exit 1; }
if [ "$GITHUB" -eq 1 ]; then
 command -v gh >/dev/null || { echo 'sudo apt install gh' >&2; exit 1; }
 gh auth status >/dev/null || { echo 'Run gh auth login first' >&2; exit 1; }
 ACCOUNT="$(gh api user --jq '.login')"
 if [ "${ACCOUNT,,}" != 'tomsellecksmustache' ]; then
   echo "GitHub account is $ACCOUNT, expected TomSellecksMustache. Refusing." >&2; exit 1
 fi
 if gh repo view TomSellecksMustache/Lufia-II-NX >/dev/null 2>&1; then
   echo 'Target GitHub repo already exists. Refusing to overwrite.' >&2; exit 1
 fi
fi
mkdir -p -- "$PROJECT"
# Only bundled, ROM-free files are copied to the GitHub checkout.
cp -a -- "$SCRIPT_DIR/." "$PROJECT/"
cd -- "$PROJECT"
git init -b main
git config --get user.name >/dev/null || { echo 'Set git config --global user.name "Your Name" first' >&2; exit 1; }
git config --get user.email >/dev/null || { echo 'Set git config --global user.email "you@example.com" first' >&2; exit 1; }
python3 tools/nxctl.py rom "$ROM"
python3 tools/nxctl.py test
# Initial add must pass content scan. ROM cannot be included even with force-add.
git add -- .gitignore .github LICENSE README.md VERSION bootstrap.sh scripts tools ports docs tests
git diff --cached --check
python3 tools/nxctl.py audit
# Create and tag a gate-verified local v0.0.1 without any ROM or generated C.
python3 tools/nxctl.py milestone 0.0.1
if [ "$GITHUB" -eq 1 ]; then
 if [ "$PUBLIC" -eq 1 ]; then VISIBILITY='--public'; else VISIBILITY='--private'; fi
 gh repo create TomSellecksMustache/Lufia-II-NX "$VISIBILITY" --source=. --remote=origin --push
 git push origin v0.0.1
 gh release create v0.0.1 --repo TomSellecksMustache/Lufia-II-NX \
  --title 'v0.0.1 - ROM-free scaffold' --notes-file docs/RELEASE_NOTES.md --prerelease
fi
cat <<NEXT

Created: $PROJECT
Completed: local v0.0.1

Next:
  cd "$PROJECT"
  ./scripts/install-toolchain.sh   # optional system dependencies, inspect script first
  python3 tools/nxctl.py iterate --until 0.5.0  # stops on real hardware checks
  python3 tools/nxctl.py status

To create/publish GitHub after a local-only bootstrap:
  ./scripts/publish-initial.sh

WARNING: 0.6.0-0.8.0 are gated on a REAL game bridge, not generated automatically.
NEXT
