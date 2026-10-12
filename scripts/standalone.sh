#!/usr/bin/env bash
# SiNXstrals standalone 0.1.0: future ROM-free NRO/ZIP build and publish gate.
set -Eeuo pipefail
MODE="${1:-check}"                 # check | build | publish
ROOT="$(git rev-parse --show-toplevel)"
SRC="$ROOT/ports/standalone"
CACHE="${LUFIA2NX_CACHE:-$HOME/.cache/lufia2nx}"
BUILD="$CACHE/standalone-build"
STAGE="$HOME/Downloads/shared/Lufia-II-NX/switch"
VERSION="$(cat "$ROOT/VERSION")"
ZIP="$STAGE/SiNXstrals-standalone-v${VERSION}-Switch.zip"
TAG="standalone-v${VERSION}"
ICON="$ROOT/assets/wyzecorp-standalone-icon.jpg"
fail(){ echo "STOP: $*" >&2; exit 1; }
[[ "$MODE" == check || "$MODE" == build || "$MODE" == publish ]] || fail 'Use check, build or publish.'
[[ "$(git -C "$ROOT" branch --show-current)" == standalone ]] || fail 'Switch to standalone branch first.'
if [[ "$MODE" != publish ]]; then
  [[ -s "$SRC/CMakeLists.txt" && -d "$SRC/src" ]] || fail 'Standalone 65816 execution backend not yet implemented. No fake NRO will be built.'
  [[ -s "$SRC/THIRD_PARTY_ATTRIBUTION.md" ]] || fail 'Review and include required native-runtime license notices.'
  if grep -RIlE 'native-switch-source|lufia2-gen|snes9x|libretro' "$SRC" --include='*.cmake' --include='*.txt' --include='*.c' --include='*.h' | grep -q .; then
    fail 'Prohibited generated/private-code reference in standalone runtime.'
  fi
  [[ "$MODE" == check ]] && { echo 'Standalone source gate passed; gameplay still requires hardware verification.'; exit 0; }
  [[ -s "$ICON" ]] || fail 'Supply an ORIGINAL redistributable icon: assets/wyzecorp-standalone-icon.jpg'
  export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
  export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
  export PATH="$DEVKITPRO/tools/bin:$DEVKITA64/bin:$PATH"
  for c in cmake ninja nacptool elf2nro aarch64-none-elf-readelf python3; do
    command -v "$c" >/dev/null || fail "Missing $c"
  done
  mkdir -p "$BUILD" "$STAGE"
  cmake -S "$SRC" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,-Map=$BUILD/standalone.map"
  cmake --build "$BUILD" --target SiNXstralsStandalone --parallel "${JOBS:-4}"
  ELF="$BUILD/SiNXstralsStandalone.elf"
  [[ -s "$ELF" ]] || fail 'Standalone ARM64 ELF missing.'
  aarch64-none-elf-readelf -h "$ELF" | grep -q 'Machine:.*AArch64' || fail 'Wrong architecture.'
  LC_ALL=C grep -aFq 'SINXSTRALS_STANDALONE_ROMFREE' "$ELF" || fail 'Standalone runtime marker missing.'
  python3 - "$BUILD/compile_commands.json" "$BUILD/standalone.map" <<'PY'
import json,sys
from pathlib import Path
cc,mp=map(Path,sys.argv[1:])
if not cc.is_file() or not mp.is_file():
    raise SystemExit('Missing compiler provenance files.')
items=json.loads(cc.read_text())
if not items: raise SystemExit('No compiled native sources.')
text='\n'.join(str(x) for x in items)+mp.read_text(errors='replace')
for forbidden in ('native-switch-source','lufia2-gen','lufia2-decomp','snes9x','libretro'):
    if forbidden.lower() in text.lower():
        raise SystemExit('Forbidden generated/backend dependency: '+forbidden)
print('Preliminary ROM-free compiler/linker check passed.')
PY
  TMP="$(mktemp -d "$BUILD/.package.XXXXXXXX")"
  trap 'rm -rf -- "$TMP"' EXIT
  nacptool --create 'SiNXstrals' 'WyzeCorp' "$VERSION" "$TMP/game.nacp"
  elf2nro "$ELF" "$TMP/game.nro" --nacp="$TMP/game.nacp" --icon="$ICON"
  python3 - "$TMP/game.nro" "$TMP/package.zip" "$SRC/THIRD_PARTY_ATTRIBUTION.md" <<'PY'
from pathlib import Path
import zipfile,sys
nro,package,license=map(Path,sys.argv[1:])
b=nro.read_bytes()
if len(b)<4096 or b[16:20]!=b'NRO0' or b'ASET' not in b:
    raise SystemExit('Invalid NRO')
with zipfile.ZipFile(package,'w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('switch/SiNXstrals/sinxstrals.nro',b)
    z.writestr('SETUP.txt','SiNXstrals standalone\n\nExtract ZIP to SD card root. Supply your own headerless US Lufia II ROM at switch/SiNXstrals/lufia2.sfc. Launch sinxstrals.nro. No ROM included.\n')
    z.write(license,'THIRD_PARTY_ATTRIBUTION.md')
with zipfile.ZipFile(package) as z:
    assert len(z.namelist())==3
print('ROM-free ZIP structure validated.')
PY
  cp "$TMP/game.nro" "$STAGE/.standalone.$$.tmp"
  mv -f "$STAGE/.standalone.$$.tmp" "$STAGE/sinxstrals-standalone.nro"
  cp "$TMP/package.zip" "$STAGE/.standalonezip.$$.tmp"
  mv -f "$STAGE/.standalonezip.$$.tmp" "$ZIP"
  sha256sum "$ZIP" > "$BUILD/package.sha256"
  echo "Built, NOT published: $ZIP"
  echo 'Test gameplay, audio and saves on real hardware before publishing.'
  exit 0
fi
[[ "${SINX_HARDWARE_PASSED:-}" == YES ]] || fail 'Hardware not confirmed; set SINX_HARDWARE_PASSED=YES only after testing.'
[[ -s "$ZIP" && -s "$BUILD/package.sha256" ]] || fail 'Standalone package not yet built.'
sha256sum -c "$BUILD/package.sha256" || fail 'ZIP was modified after build.'
[[ -z "$(git -C "$ROOT" status --porcelain)" ]] || fail 'Uncommitted source changes.'
! git -C "$ROOT" rev-parse --verify "refs/tags/$TAG" >/dev/null 2>&1 || fail 'Release tag exists.'
! gh release view "$TAG" -R TomSellecksMustache/SiNXstrals >/dev/null 2>&1 || fail 'Release already exists.'
grep -qi 'not yet implemented' "$ROOT/README.md" && fail 'Update the README after game verification.'
(cd "$ROOT" && python3 tools/nxctl.py audit)
git -C "$ROOT" tag -a "$TAG" -m "SiNXstrals standalone $VERSION"
git -C "$ROOT" push origin standalone
git -C "$ROOT" push origin "$TAG"
gh release create "$TAG" "$ZIP" -R TomSellecksMustache/SiNXstrals \
  --title "SiNXstrals standalone $VERSION" --notes-file "$ROOT/docs/RELEASE_NOTES.md" \
  --prerelease --verify-tag
