#!/usr/bin/env bash
# Lufia-II-NX phase 2: try a genuine ARM64 static-recompilation build.
# No ROM, generated C or binary ever enters the public Git repository.
set -Eeuo pipefail
umask 077

MODE="${1:-help}"
if [[ $# -gt 0 ]]; then shift; fi
SCRIPT_PATH="$(readlink -f "${BASH_SOURCE[0]}")"
REPO="${LUFIA_NX_REPO:-$PWD}"
CACHE="${LUFIA2NX_CACHE:-$HOME/.cache/lufia2nx}"
CACHE="$(realpath -m "$CACHE")"
DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITPRO DEVKITA64="$DEVKITPRO/devkitA64"
export PATH="$DEVKITPRO/portlibs/switch/bin:$DEVKITPRO/tools/bin:$DEVKITA64/bin:$PATH"
UPSTREAM="$CACHE/upstream/Lufia2SNESRecomp"
SDL_SOURCE="$CACHE/vendor/sdl3-source-3.4.10"
SDL_PATCH="$CACHE/vendor/sdl3-switch"
PRIVATE_SOURCE="$CACHE/native-switch-source"
BUILD="$CACHE/native-switch-build"
ARTIFACTS="$CACHE/artifacts"
LOGS="$CACHE/logs"
EXPECTED="7c34ecb16c10f551120ed7b86cfbc947042f479b52ee74bb3c40e92fdd192b3a"
ROM="${LUFIA2_ROM:-}"
SDL_VERSION=release-3.4.10
mkdir -p "$CACHE" "$LOGS"

fail(){ printf '\nERROR: %s\n' "$*" >&2; exit 1; }
log(){ printf '\n>>> %s\n' "$*" >&2; }
need(){ command -v "$1" >/dev/null || fail "Missing tool: $1"; }
private_path(){
  local rp
  rp="$(realpath -m "$1")"
  local source_root="$(realpath -m "$REPO")"
  [[ "$rp" != "$source_root" && "$rp" != "$source_root/"* ]] || fail "Private artifact path cannot be inside public repo: $rp"
}

help_text(){ cat <<'EOF'
Lufia-II-NX phase 2 native ARM64 integration experiment

Usage: bash phase2-native.sh preflight
       bash phase2-native.sh desktop [--run]
       bash phase2-native.sh switch
       bash phase2-native.sh install /absolute/path/to/mounted/SD [--copy-rom]

Options via environment:
  LUFIA_NX_REPO=~/src/Lufia-II-NX
  LUFIA2_ROM=/absolute/path/to/original/lufia2.sfc
  LUFIA2NX_CACHE=~/.cache/lufia2nx

A successful 'switch' build yields a PRIVATE native .nro for testing.
An unsuccessful build preserves a failure log. No tags/releases/pushes.
EOF
}
get_rom(){
  need python3
  local rompath
  rompath="$(python3 - "$CACHE" "$ROM" <<'PY'
import json,pathlib,sys,os
cache,override=sys.argv[1:]
if override: print(pathlib.Path(override).expanduser().resolve())
else:
  s=pathlib.Path(cache)/'private_state.json'
  if s.exists():print(pathlib.Path(json.loads(s.read_text()).get('rom_path','')).expanduser().resolve())
PY
)"
  [[ -n "$rompath" && -f "$rompath" ]] || fail "ROM not configured. Set LUFIA2_ROM=/absolute/path/...sfc or run nxctl.py rom"
  private_path "$rompath"
  [[ "$(stat -c %s "$rompath")" == 2621440 ]] || fail "Unsupported ROM file size"
  local sha
  sha="$(sha256sum "$rompath" | cut -d' ' -f1)"
  [[ "$sha" == "$EXPECTED" ]] || fail "Unsupported ROM SHA-256"
  ROM="$rompath"
  log "Correct USA ROM verified from private path; no ROM is being copied to the Git tree"
}
check_repo(){
  [[ -f "$REPO/tools/nxctl.py" && -f "$REPO/ports/switch/source/game_bridge.c" ]] || fail "Run from Lufia-II-NX repository root, or set LUFIA_NX_REPO"
  private_path "$CACHE"
  [[ "$CACHE" != "/" && "$CACHE" != "$HOME" ]] || fail "Unsafe cache location"
  private_path "$UPSTREAM"
  private_path "$PRIVATE_SOURCE"
  private_path "$ARTIFACTS"
  get_rom
  if command -v git >/dev/null && [[ -d "$REPO/.git" ]]; then
    ( cd "$REPO" && git status --short && python3 tools/nxctl.py audit )
  fi
}
check_toolchain(){
  [[ -f "$DEVKITPRO/cmake/Switch.cmake" ]] || fail "Missing $DEVKITPRO/cmake/Switch.cmake"
  [[ -f "$DEVKITPRO/libnx/switch_rules" ]] || fail "Missing libnx"
  need aarch64-none-elf-gcc
  need elf2nro
  need nacptool
}
clone_once(){
  local url="$1" target="$2"; shift 2
  if [[ ! -d "$target/.git" ]]; then
    mkdir -p "$(dirname "$target")"
    git clone "$@" "$url" "$target"
  else
    log "Reusing existing checkout: $target (no silent updates)"
  fi
  log "Pinned locally: $(git -C "$target" rev-parse --short HEAD) $url"
}
prepare_upstream(){
  need git
  clone_once https://github.com/Cellenseres/Lufia2SNESRecomp.git "$UPSTREAM" --recursive
  git -C "$UPSTREAM" submodule update --init --recursive
  [[ -f "$UPSTREAM/CMakeLists.txt" ]] || fail "Upstream CMake file not found"
}
preflight(){
  check_repo
  for cmd in bash python3 git cmake ninja; do need "$cmd"; done
  check_toolchain
  log "Toolchain available: $(aarch64-none-elf-gcc -dumpversion)"
  log "Preflight passed. Current public repo has NOT been modified."
}
desktop(){
  check_repo
  for cmd in git cmake ninja python3; do need "$cmd"; done
  prepare_upstream
  local output="$CACHE/desktop-build"
  mkdir -p "$output"
  log "Building genuine upstream game on Ubuntu (sources/build remain private)"
  (set -o pipefail; cmake -S "$UPSTREAM" -B "$output" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLUFIA2_ROM="$ROM" 2>&1 | tee "$LOGS/desktop-configure.log")
  (set -o pipefail; cmake --build "$output" --parallel "$(nproc)" 2>&1 | tee "$LOGS/desktop-build.log")
  [[ -x "$output/Lufia2Recomp" ]] || fail "Desktop native executable not produced"
  log "Desktop build OK: $output/Lufia2Recomp"
  if [[ "${1:-}" == '--run' ]]; then
    (cd "$output"; ./Lufia2Recomp "$ROM")
  else
    echo "Test title and controls with: bash '$SCRIPT_PATH' desktop --run"
  fi
}
prepare_sdl(){
  clone_once https://github.com/libsdl-org/SDL.git "$SDL_SOURCE" --depth 1 --branch "$SDL_VERSION"
  clone_once https://github.com/neomody77/sdl3-switch.git "$SDL_PATCH" --depth 1
  [[ -f "$SDL_PATCH/sdl3-switch.patch" ]] || fail "Expected SDL3 homebrew patch absent"
  # The third-party patch was generated against SDL release-3.4.10.
  # Refuse to apply it to another baseline or a modified SDL source.
  if [[ ! -f "$SDL_SOURCE/.lufia2nx-switch-patch-applied" ]]; then
    local base_blob
    base_blob="$(git -C "$SDL_SOURCE" hash-object CMakeLists.txt)"
    [[ "$base_blob" == faf6d90* ]] || fail "Wrong SDL3 source baseline ($base_blob). Expected CMakeLists.txt blob faf6d90 from release-3.4.10."

    if ! git -C "$SDL_SOURCE" apply -p3 --check "$SDL_PATCH/sdl3-switch.patch"; then
      fail "SDL3 Switch patch does not apply cleanly to $SDL_VERSION. See $SDL_PATCH/README.md; do not force apply."
    fi
    git -C "$SDL_SOURCE" apply -p3 "$SDL_PATCH/sdl3-switch.patch"
    date -u > "$SDL_SOURCE/.lufia2nx-switch-patch-applied"
  fi
}
prepare_private_game(){
  [[ -f "$UPSTREAM/CMakeLists.txt" ]] || fail "Upstream not cloned"
  # Always rebuild a separate local worktree. No Git metadata or public push.
  rm -rf "$PRIVATE_SOURCE"
  mkdir -p "$PRIVATE_SOURCE"
  cp -a "$UPSTREAM/." "$PRIVATE_SOURCE/"
  rm -rf "$PRIVATE_SOURCE/.git" # Avoid accidentally pushing generated C or ROM-derived build data.
  # Preserve the original checkout unmodified; only patch private copy.
  # Use gitignore semantics only as a secondary guard; primary is path isolation.
  python3 - "$PRIVATE_SOURCE" <<'PY'
from pathlib import Path
import re,sys
root=Path(sys.argv[1]); cm=root/'CMakeLists.txt'
s=cm.read_text()
def replace_exact(old,new):
 global s
 if s.count(old)!=1: raise SystemExit(f"Unrecognized upstream CMake layout; expected once: {old}")
 s=s.replace(old,new)
replace_exact('set(SDL_SHARED ON CACHE BOOL "" FORCE)', 'set(SDL_SHARED OFF CACHE BOOL "" FORCE)')
replace_exact('set(SDL_STATIC OFF CACHE BOOL "" FORCE)', 'set(SDL_STATIC ON CACHE BOOL "" FORCE)')
replace_exact('set(SNESRECOMP_PLATFORM_ENABLE_OPENGL ON CACHE BOOL "" FORCE)', 'set(SNESRECOMP_PLATFORM_ENABLE_OPENGL OFF CACHE BOOL "" FORCE)')
# Switch-only: GLSL presets target the desktop GL 3.3 presenter.
# Leave the game's rendering policy untouched and omit only this adapter.
replace_exact(
    'snesrecomp_platform_target_glsl_presets(Lufia2Recomp)',
    '''if(SNESRECOMP_PLATFORM_ENABLE_OPENGL)
    snesrecomp_platform_target_glsl_presets(Lufia2Recomp)
endif()'''
)
# Preserve full native game loop; swap the platform-specific main entry only.
old='set(LUFIA2_HOST_SOURCES\n    src/main.c'
if old in s:
 s=s.replace(old,'set(LUFIA2_HOST_SOURCES\n    src/main.c\n    src/switch_entry.c',1)
else:
 m=re.search(r'(set\(LUFIA2_HOST_SOURCES\s*\n\s*src/main\.c)',s)
 if not m:raise SystemExit('Could not find host C source list')
 s=s[:m.end()]+'\n    src/switch_entry.c'+s[m.end():]
# Add platform libraries after upstream target creation; place full link line in group.
s+='''
# Local, private Switch cross-compilation overlay (not committed upstream).
if(SWITCH OR CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
  target_compile_definitions(Lufia2Recomp PRIVATE __SWITCH__=1)
  target_link_options(Lufia2Recomp PRIVATE "-Wl,--start-group")
  target_link_libraries(Lufia2Recomp PRIVATE
    EGL GLESv2 glapi drm_nouveau stdc++ nx m z
    "-Wl,--end-group")
endif()
'''
cm.write_text(s)
p=root/'src/main.c'; text=p.read_text()
matches=list(re.finditer(r'\bint\s+main\s*\(\s*int\s+argc\s*,\s*char\s*\*\s*\*\s*argv\s*\)',text))
if len(matches)!=1:raise SystemExit(f'Expected one upstream int main(int argc,char **argv); saw {len(matches)}')
text=text[:matches[0].start()]+text[matches[0].start():].replace('main(', 'lufia2_desktop_entry(',1)
p.write_text(text)
(root/'src/switch_entry.c').write_text(r'''/* Local Switch entry, privately built; calls original Lufia II native host. */
#include <switch.h>
#include <stdio.h>
#include <unistd.h>
extern int lufia2_desktop_entry(int argc, char **argv);
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *home="sdmc:/switch/Lufia-II-NX";
    (void)chdir(home);
    char *args[]={(char *)"Lufia-II-NX", (char *)"sdmc:/switch/Lufia-II-NX/lufia2.sfc", 0};
    return lufia2_desktop_entry(2,args);
}
''')
print('Local Switch-specific game entry prepared. Original game runtime preserved.')
PY
}
run_switch(){
  check_repo
  for cmd in git cmake ninja python3; do need "$cmd"; done
  check_toolchain
  prepare_upstream
  prepare_sdl
  prepare_private_game
  # Current upstream desktop path still uses shared SDL, GL presentation and
  # recomp-ui desktop facilities. This experimental cross-build reveals the
  # actual portability blockers. Fail closed rather than releasing a fake game.
  rm -rf "$BUILD"
  mkdir -p "$BUILD" "$ARTIFACTS"
  log "Configuring actual game source against experimental SDL3 Switch backend"
  if ! (set -o pipefail; cmake -S "$PRIVATE_SOURCE" -B "$BUILD" -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
      -DCMAKE_BUILD_TYPE=Release \
      -DLUFIA2_ROM="$ROM" \
      -DLUFIA2_SDL3_ROOT="$SDL_SOURCE" \
      -DSNESRECOMP_PLATFORM_ENABLE_OPENGL=OFF \
      -DLUFIA2_ENABLE_VULKAN=OFF \
      -DLUFIA2_ENABLE_REVIEWED_AOT=ON \
      -DLUFIA2_ENABLE_DECOMP=ON \
      -DSDL_SHARED=OFF -DSDL_STATIC=ON \
      2>&1 | tee "$LOGS/phase2-configure.log"); then
      fail "Switch configure failed. Inspect: $LOGS/phase2-configure.log"
  fi
  log "Cross-compiling the complete Lufia II runtime, not a placeholder title"
  if ! (set -o pipefail; cmake --build "$BUILD" --target Lufia2Recomp --parallel "$(nproc)" \
      2>&1 | tee "$LOGS/phase2-build.log"); then
      fail "Native engine cross-build failed; inspect $LOGS/phase2-build.log. No NRO/tag/release created."
  fi
  local elf="$BUILD/Lufia2Recomp"
  [[ -f "$elf" ]] || elf="$BUILD/Lufia2Recomp.elf"
  [[ -f "$elf" ]] || fail "Missing ARM64 game executable after build"
  aarch64-none-elf-readelf -h "$elf" | grep -q 'AArch64' || fail "Output is NOT AArch64"
  nacptool --create "Lufia II NX" "Local Homebrew" "0.6.0-dev" "$ARTIFACTS/lufia2nx-native.nacp"
  elf2nro "$elf" "$ARTIFACTS/lufia2nx-native.nro" --nacp="$ARTIFACTS/lufia2nx-native.nacp"
  [[ -s "$ARTIFACTS/lufia2nx-native.nro" ]] || fail "NRO missing or empty"
  log "PRIVATE native Switch game attempt built: $ARTIFACTS/lufia2nx-native.nro"
  log "Game still requires verification on real Switch. Do NOT publish this artifact."
}
install_sd(){
  local sd="${1:-}" opt="${2:-}"
  [[ -n "$sd" && -d "$sd" ]] || fail "Supply your mounted SD card root as first argument"
  [[ "$sd" == /* ]] || fail "SD path must be absolute"
  local nro="$ARTIFACTS/lufia2nx-native.nro"
  [[ -f "$nro" ]] || fail "No private native game NRO built yet. Run 'switch'."
  get_rom
  local dst="$sd/switch/Lufia-II-NX"
  mkdir -p "$dst/saves"
  cp "$nro" "$dst/lufia2nx-native.nro"
  if [[ "$opt" == '--copy-rom' ]]; then
    cp "$ROM" "$dst/lufia2.sfc"
    log "ROM copied to local SD ONLY. Not uploaded to GitHub."
  else
    log "ROM NOT copied. Existing $dst/lufia2.sfc required on Switch."
  fi
  log "PRIVATE NRO installed to $dst/lufia2nx-native.nro"
}
case "$MODE" in
  help|-h|--help) help_text;;
  preflight) preflight;;
  desktop) desktop "${1:-}";;
  switch) run_switch;;
  install) install_sd "${1:-}" "${2:-}";;
  *) help_text; fail "Unknown mode $MODE";;
esac
