#!/usr/bin/env bash
# Local-only build of the ROM-derived native edition. Never upload this NRO.
set -Eeuo pipefail
umask 077
CACHE="${LUFIA2NX_CACHE:-$HOME/.cache/lufia2nx}"
BUILD="$CACHE/native-switch-build"
SOURCE="$CACHE/native-switch-source"
WORK="$HOME/Downloads/shared/Lufia-II-NX"
DEST="$WORK/switch/lufia2nx-native.nro"
ICON="$WORK/assets/lufia-nx-icon.jpg"
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
export PATH="$DEVKITPRO/tools/bin:$DEVKITA64/bin:$DEVKITPRO/portlibs/switch/bin:$PATH"
stop(){ printf 'STOP: %s\n' "$*" >&2; exit 1; }
for c in cmake ninja nacptool elf2nro aarch64-none-elf-readelf python3; do
  command -v "$c" >/dev/null || stop "Missing $c"
done
[[ -s "$BUILD/build.ninja" && -s "$SOURCE/src/main.c" ]] || stop 'Private native workspace not prepared.'
[[ -s "$ICON" ]] || stop "Local NRO icon missing: $ICON"
mkdir -p "$WORK/switch" "$CACHE/artifacts"
cmake --build "$BUILD" --target Lufia2Recomp --parallel "${JOBS:-4}"
ELF="$BUILD/Lufia2Recomp.elf"
[[ -s "$ELF" ]] || stop 'Native ELF missing.'
aarch64-none-elf-readelf -h "$ELF" | grep -q 'Machine:.*AArch64' || stop 'Wrong ELF architecture.'
TMP="$(mktemp -d "$CACHE/artifacts/.sinx-066.XXXXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT
nacptool --create 'SiNXstrals' 'WyzeCorp' '0.6.6' "$TMP/game.nacp"
elf2nro "$ELF" "$TMP/game.nro" --nacp="$TMP/game.nacp" --icon="$ICON"
python3 - "$TMP/game.nro" <<'PY'
from pathlib import Path
import sys
b=Path(sys.argv[1]).read_bytes()
if len(b)<4096 or b[16:20]!=b'NRO0' or b'ASET' not in b:
    raise SystemExit('Invalid NRO. Old staged build remains intact.')
print(f'Validated private NRO: {len(b):,} bytes')
PY
if [[ -s "$DEST" ]]; then cp -p "$DEST" "$CACHE/artifacts/sinxstrals-before-066.nro"; fi
cp "$TMP/game.nro" "$WORK/switch/.sinxstrals-066.$$.tmp"
mv -f "$WORK/switch/.sinxstrals-066.$$.tmp" "$DEST"
printf 'PRIVATE NRO: %s\n' "$DEST"
sha256sum "$DEST"
