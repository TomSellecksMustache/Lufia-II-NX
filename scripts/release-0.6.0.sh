#!/usr/bin/env bash
# Lufia-II-NX 0.6.0: guarded private Switch build + ROM-free GitHub source release.
# Place this ONE script in ~/Downloads/shared/Lufia-II-NX and run it from there.
# Does NOT upload the ROM, recompiled objects, generated C or native game binary.
set -Eeuo pipefail
umask 077
VERSION=0.6.0
TAG=v0.6.0
REPO_NAME=TomSellecksMustache/Lufia-II-NX
WORK="$HOME/Downloads/shared/Lufia-II-NX"
REPO="${LUFIA_NX_REPO:-$HOME/src/Lufia-II-NX}"
CACHE="${LUFIA2NX_CACHE:-$HOME/.cache/lufia2nx}"
SRC="$CACHE/native-switch-source"
BUILD="$CACHE/native-switch-build"
STAGE="$HOME/Downloads/switch/switch/Lufia-II-NX"
ART="$CACHE/artifacts"
LOG="$CACHE/logs/release-0.6.0-build.log"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
MODE="${1:-release}"
fail() { printf '\nERROR: %s\n' "$*" >&2; exit 1; }
note() { printf '\n>>> %s\n' "$*"; }
[[ "$SCRIPT_DIR" == "$WORK" ]] || fail "Put this script in $WORK and execute it from there."
[[ "$PWD" == "$WORK" ]] || fail "Run: cd $WORK && bash ./lufia-nx-release-0.6.0.sh"
case "$MODE" in release|check) ;; *) fail 'Usage: bash ./lufia-nx-release-0.6.0.sh [check|release]' ;; esac
for tool in git python3 cmake ninja nacptool elf2nro aarch64-none-elf-readelf sha256sum gh; do
  : # Check tool availability after setting the Switch environment.
done
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
export PATH="$DEVKITPRO/portlibs/switch/bin:$DEVKITPRO/tools/bin:$DEVKITA64/bin:$PATH"
for tool in git python3 cmake ninja nacptool elf2nro aarch64-none-elf-readelf sha256sum gh; do
  command -v "$tool" >/dev/null || fail "Missing tool: $tool (install it first; GitHub CLI: sudo apt install gh)"
done
[[ -d "$REPO/.git" ]] || fail "Git repo not found: $REPO"
[[ -f "$BUILD/build.ninja" && -f "$SRC/src/main.c" && -f "$SRC/src/lufia2_splash_credit.c" ]] ||
  fail 'Existing, verified private native Switch build is missing. Do not run a fresh build/reclone.'
[[ "$(realpath -m "$REPO")" != "$(realpath -m "$CACHE")" ]] || fail 'Repository/cache collision.'
[[ "$(realpath -m "$CACHE")" != "$(realpath -m "$REPO")/"* ]] || fail 'Private cache is inside public repository.'
# Permit only modifications created by a previous interrupted release attempt, and
# the local Phase 2 script that was installed after the starter repository.
python3 - "$REPO" <<'PY_STATUS'
import subprocess,sys
repo=sys.argv[1]
allowed={'README.md','docs/RELEASE_NOTES.md','VERSION','tools/nxctl.py',
         'scripts/release-0.6.0.sh','scripts/phase2-native.sh',
         'assets/wyzecorp-logo.png','assets/wyzecorp-publisher.png'}
proc=subprocess.run(['git','-C',repo,'status','--porcelain','-z'],capture_output=True)
if proc.returncode:raise SystemExit('git status failed')
items=proc.stdout.decode('utf-8','replace').split('\0')
for item in items:
    if not item:continue
    if item[3:] not in allowed:
        raise SystemExit('Unrelated Git change; commit/stash it before release: '+item)
PY_STATUS
[[ -f "$REPO/scripts/phase2-native.sh" ]] || fail 'The project-native Phase 2 build script is missing from scripts/. Cannot document a build command that does not exist.'
remote="$(git -C "$REPO" remote get-url origin)"
[[ "$remote" == *"TomSellecksMustache/Lufia-II-NX" || "$remote" == *"TomSellecksMustache/Lufia-II-NX.git" ]] || fail "Unexpected GitHub origin: $remote"
[[ "$(gh api user --jq .login | tr '[:upper:]' '[:lower:]')" == 'tomsellecksmustache' ]] || fail 'GitHub CLI must be authenticated as TomSellecksMustache (gh auth login).'

gh repo view "$REPO_NAME" --json nameWithOwner >/dev/null || fail "Cannot access GitHub repo $REPO_NAME"
if gh release view "$TAG" --repo "$REPO_NAME" >/dev/null 2>&1; then
    fail "GitHub release $TAG already exists. Refusing to overwrite or retag a published version."
fi
if git -C "$REPO" rev-parse --verify -q "refs/tags/$TAG" >/dev/null; then
    fail "Local tag $TAG already exists. Investigate it before publishing; don't rewrite tags."
fi
if git -C "$REPO" ls-remote --exit-code --tags origin "refs/tags/$TAG" >/dev/null 2>&1; then
    fail "GitHub tag $TAG already exists. Refusing to overwrite."
fi
note 'Preflight passed. Native game remains private; GitHub release will contain source and original WyzeCorp branding only.'
if [[ "$MODE" == check ]]; then
  printf 'Repository: %s\nPrivate cache: %s\nLocal output: %s\n' "$REPO" "$CACHE" "$STAGE"
  exit 0
fi
mkdir -p "$CACHE/logs" "$ART" "$STAGE" "$REPO/assets" "$REPO/docs" "$REPO/scripts"

# Write original owned logo, screenshot-style publisher card and the small
# compressed pixel header solely from the embedded, known-good assets.
# The C header is placed PRIVATE under the cached game source, not committed.
python3 - "$SRC/src/main.c" "$SRC/src/lufia2_splash_credit.c" "$REPO" "$CACHE" <<'PY'
from pathlib import Path
import sys,base64,hashlib,re
main,splash,repo,cache=map(Path,sys.argv[1:])
logo=base64.b64decode('''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''')
card=base64.b64decode('''iVBORw0KGgoAAAANSUhEUgAAAQAAAADgCAIAAABjIy8HAAAMxElEQVR4nO3deXgU9RnA8V8QEZQI
KgmH2oZwIzEYNYocIZDGKC2gqBwKSEBUvC1StTxKvAChqfejtULlMoJgBIW4ECBGrphGbhJyGEkQ
jGK5tCTPI/SP0XG7M5mdmT1mdub7+YMn7M478+7uvL/fO0c2QgAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAMABYmJiV63Z4PPg+ee38qzfLP2rM0TbpMlT
JmTebT5LNY2lF7jgZqsnT+231MQbbrkmVicghBBnNW1aUV1XUV23a9/Xi3M+7NDhEkPhx48fSx90
XYhyM0R6IVnPvSj9d9LkKQ8+/Jh2ehdceOEaT2FIs5o67a9bi/cUf1n21IwXWpx7bmOLyXmGISX7
sEUBCCEaGho6x8UmJ3X/qqrynvsesjod83768cfk5D7x8Z2tTuQX11zb949Dbx5z27BBKcmVleX9
+qVYnZG92KUAJGfOnDl95owQolOnLkvez5UevOfeB6WJvulZTedmv759d+XCJcvbtImRo+S5OyYm
dunyT/aV11ZU140dP1EKmfniS9t3Vy55P7dly2hp+bHjJ362uWTHnqqXXn3rnHPOEUJM/cv0kp3l
H+dtVO64U+5/ZGvxnuLtZU9nzWzSpElMTOwaT6FynbKfT/+cPfeFaU88pZqeMvblV9/u0rVbRXXd
m28vUOamGqLM1idJ73xOnjzxw5Hvv/mm9vjxY4sXzl/rWTMh8+5Jk6cIIbKenZ2zbJUQImXg4Flz
XpbzVKZUsKmk/KtvCzaVNPaWynw+o3vve/juex+QnnpyetbI0WPXeAoTLu+tY18IE7sUQLNmzSqq
64q37+/Spetbb7ysukx8fKfcD5f2SU7YvWvng49MUy7Qb8DA2toDCT06do6LXfjuO0KIuI4dc1cs
63N1r8OHD90wZKgQIrF30vCbbh054k/XXnXZd9/VjbljQvI116Wm/mHI9QMn3jm6V0Ki9wqvTr52
2PARt9x0Q0Zav27dewwdPkJ1nT7Weta0atX66uQ+yqeUsQ89cFf5/rLOcbH33DVOmZsyRJmtapKy
Pbt3vvbK3555fs7c7Nf//NiT0dHnF2zMH5iaJoTolZBYX1/fqlXrASmDCjbmyyHeKV2RdNW4OydN
vHN0t07tU/om+X35Pp/RkkX/unnEyLOaNj33vPNSB6fnrlh6Q3r/XTu3q36+lmhqdQK/aGho6Nn1
t9bfeySO+nVIq6k58HlhgRBi+Qc5zz4/R7mStZ+uufLK5EXvrTh4sOb1V7JPnDheVVm5besmIURx
0daYmFghxGWXJST2Tirc8qUUsmzpkoaG+g3rPYcOHRRCrFub573Crt16rM9fW1tzQAix+pOVXbv2
2FRYoFyn0qznZzz1zAt5q1f5PK4dq8xNGdKsWTOfbJVJ+qx2fb5nfb5HCHH72AlPZ82c+uh9sW3b
JSYmVVVWlJbu7T8gtV//lJeyZ0dFRSlfSM+evdav+7SivEznS/D5jI4dO/p5YcH1GUNiYmJzVyyr
r69XfbssZJcZwMeRI9/HdezUtl372Lbt0tIzpAcvvfR31/Xt36JFi5tuvq3c6yORnTx5YvqTU8eM
HLZje8lDj04TQpw+c1p+VvqA9+3bU/zFtpS+SV06tu0cF/vEtIfL95elpKa1a98htm27wWnXe69w
f1lp6qC0iy++5KKL2tw4ZGh5eanqOpV27CiprTlw45BhPo8rY0+dOhUdHd28eXPV3JQhymxVk5T1
H5A65f5H2rXv0LJldFzH+NYXXCCE2LKp8InpWRs3rCvYmH/HuMwffjhy4sRxOcQ7pb17dw9Oy+jc
pZv8SrVfvvIzmv/Om+PGT7p15O2LF85Xfa+sZdMCOHbs6NKcRZ78Tf/458KyfXulB6uqKkfcMnrL
F7sTe1/xyt9fVEZNmjylorpuX/nBO8Zlvrd4geqavywpXvXR8sU5uWWVhyqq68bcPr5o2+aCjfmr
Py2Y927Ont07vRf+omjLJx9/tGKlx7N+c3n5/pW5y/W/hDmzn+veo6ffxf77009FRVtKdla8+fYC
ZW7K5ZXZaie5beums88++4MVq7cU7erVK3H2zCwhRMHG/CuSrir8bENFeVn7DhcXbMj3DvFJadHC
efMX5OyvOiwdA2hTfka1tTV13x4u2rr56NH/CCHsdgwAhFbLltEfrvRc+rvfW52IOpvOAHCGhMt7
/3t7WdG2zTUHvrY6FwAAAAAAAAAAAAAAAAAAAAAAAAAAAACRSP1rbSLR90dPmQts07p5cDOJFPNy
1pkLzByVFtxMLGSXb4YLqZV5n1udgh0NzejX2FPuecf4WhS4mnMKQKOT0RjqRACdQETT6Bi1h38n
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
aixsDrhtcAAAAABJRU5ErkJggg==''')
assert hashlib.sha256(logo).hexdigest()=='ab2bfd07fac8a476aac8a7534bbf446113dcb901459bf28a805f0acbd73e85d8'
assert hashlib.sha256(card).hexdigest()=='5d72a053570e36a31637978231a2390875977f2f1d2f1f890c8c7b986f6cdfa8'
# Only exact, known owned assets are allowed in the public repository.
for name,data in [('wyzecorp-logo.png',logo),('wyzecorp-publisher.png',card)]:
    path=repo/'assets'/name
    if path.exists() and path.read_bytes()!=data:
        raise SystemExit(f'Refusing to overwrite a different public branding asset: {path}')

marker='L2NX_PUBLISHER_060'
original=main.read_text(); s=original
inc='''/* L2NX_PUBLISHER_060: original WyzeCorp publisher card, Switch only. */
#if defined(__SWITCH__)
#include "lufia2_nx_publisher_card.h"
#endif
'''
anchor='static bool InitAudio(void) {'
if marker not in s:
    if s.count(anchor)!=1 or s.count('    if (!InitAudio()) {')!=1:
        raise SystemExit('Unexpected main.c startup layout; no sources modified.')
    s=s.replace(anchor,inc+'\n'+anchor,1)
    s=s.replace('    if (!InitAudio()) {',
       '    /* L2NX_PUBLISHER_060: before starting audio/game frames. */\n'
       '#if defined(__SWITCH__)\n'
       '    Lufia2NxShowPublisherCard(s_presenter);\n'
       '#endif\n'
       '    if (!InitAudio()) {',1)
else:
    if s.count('Lufia2NxShowPublisherCard(s_presenter);')!=1:
        raise SystemExit('Publisher marker present but startup call missing; not overwriting.')

orig_splash=splash.read_text();ss=orig_splash
start='void Lufia2SplashCreditPrepare(Ppu *ppu) {'
if marker not in ss:
    if ss.count(start)!=1:
        raise SystemExit('Unknown splash-credit function. Aborting instead of clobbering upstream.')
    ss=ss.replace(start, start+'''
#if defined(__SWITCH__)
    /* L2NX_PUBLISHER_060: the full-colour publisher card runs before the game.
       Prevent the earlier text-only credit from appearing in its place. */
    Lufia2SplashCreditRelease(ppu);
    return;
#endif
''',1)
else:
    if ss.count('Lufia2SplashCreditRelease(ppu);')<1:
        raise SystemExit('Existing Switch credit patch is inconsistent.')

compressed=base64.b64decode('''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
Bsg/jN3P5VSZ/376nBEMev7LqFH+oZ75r9PnjKGfxoA81/sUefyv0/eMgfzLP4zl/Odpo4zz/17X
JP8M0hhQt/zLPvKfrb/nzUpZr//3sib5R/7lX/6p+xhQt/zLPvKfrd8XkZUyr//pVU3yzyCMAXXL
v+wj/9n6f1FZqfr63ypqkn/kX/6hjmNA0TkpM/+9qgnkX/5B/uUfBmUM6DQnZee/FzWB/Ms/DFL+
u8lJFfnvRV0waGNAHfMv+8h/9fkv4vs/5R+qyX+3Oakq/1XXBYM0BuTJSVn//6OXNcGg5L+InBT9
/7/GSl0g/9ly0pi7d2us1gWD+v2gnWakzOz3si6Qf3UBQFXOn/N1xhj9EgAAAAAAAAAAoL7X3Od5
fBntdKqMdoYq6vr8PO1k/ZxAVe1Qn/xnuU407fFltNOpMtrpNDNZas/TTqe5Lbsd5F/+5R/5L6Kd
Tj8/lHV9uv28zMg58GjPS5szl9FOljrKbgf5Lzr/Rax/Ue3nObanjSN1agf5l3/5Z7DzP3ROm/U9
gm4/n56lnV7lP2195J+657+qY3Kedagi/4M0j0D+5V/+qVf+s8zh2/WDtHOBMuf8ea7/6cf8Z71u
Z7TzkTztUP/8j+XX+bI8vqj3/Psx/97/R/7lX/7pt/znOXfo5vqffjz/b3V9Tjd1+Q5h+R/L+a/y
uf34+p/X85B/+Zd/us1J2ny703Pmos7bi3q/oFe5rfr9v06u1Zd/2uU/y2fpys5/la8X1ul9+6Ie
g/zLv/wzOOf/afPt0T5v26v37ar4/p9W8+purpMpop2sue3087+yIP9j4Xju+z+KPW4XNR9B/uVf
/qmfTl9jr/Lxef7PdRn/L7uo62SKWoc829+1PQAAAAAAAAD1vgag1fvEaX+3e9+6k/fRR3tvPMu6
dPuedp51Kfu+Iq7j6KS+TrYN9cp/u4y3e2wntxXRZtq6ZWkzb+1Znl/2dily/+WtHfmvOv+tfubp
r/Iv/3WZt3d7XWm38/9e5b/dXCDvtbidzn+7yX+e5WXtB0XP/6vohzj+d3JbXef/3Zz7d1NDp2OM
rMi//Fcz/8+TTfmnqNf/O3ltuYjXufvl9f+i5v+9HMO6fY8GAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAOB/DvcpVA==''')
header=r'''/* L2NX_PUBLISHER_060: generated exclusively from WyzeCorp-owned artwork.
 * Native 256x224, 4:3 ARGB8888 publisher splash. No ROM imagery/data.
 */
#ifndef L2NX_PUBLISHER_CARD_H
#define L2NX_PUBLISHER_CARD_H
#include <zlib.h>
static const unsigned char l2nx_publisher_pixels_z[] = {%%%PIXEL_BYTES%%%};
static unsigned char l2nx_publisher_pixels[256u * 224u * 4u];
static void Lufia2NxShowPublisherCard(SnesRecompPresenter *presenter) {
    uLongf output_size = (uLongf)sizeof(l2nx_publisher_pixels);
    if (uncompress(l2nx_publisher_pixels, &output_size,
                   l2nx_publisher_pixels_z,
                   (uLong)sizeof(l2nx_publisher_pixels_z)) != Z_OK ||
                   output_size != sizeof(l2nx_publisher_pixels)) {
        fprintf(stderr, "[NX] Publisher card decode failed; skip.\n");
        return;
    }
    SnesRecompVideoFrame frame = {
        .pixels = l2nx_publisher_pixels,
        .pixel_format = SNESRECOMP_PIXEL_FORMAT_ARGB8888,
        .width = 256,
        .height = 224,
        .pitch = 256 * 4,
        .overlay = NULL,
    };
    if (!snesrecomp_presenter_present(presenter, &frame)) {
        fprintf(stderr, "[NX] Publisher card present failed; skip.\n");
        return;
    }
    SDL_Delay(1700); /* Time to read; original game is not running yet. */
}
#endif
'''.replace('%%%PIXEL_BYTES%%%',','.join(str(v) for v in compressed))
private_h=main.parent/'lufia2_nx_publisher_card.h'
# Ensure all failures happen before any source or repo content is modified.
if private_h.exists() and private_h.read_text()!=header:
    raise SystemExit('Existing private publisher header differs; aborting rather than overwriting.')
for name,data in [('wyzecorp-logo.png',logo),('wyzecorp-publisher.png',card)]:
    (repo/'assets'/name).write_bytes(data)
for path,old,new in [(main,original,s),(splash,orig_splash,ss)]:
    if new!=old:
        backup=path.with_name(path.name+'.before-l2nx-publisher-060')
        if backup.exists(): raise SystemExit(f'Source backup collision: {backup}')
        backup.write_text(old)
        path.write_text(new)
private_h.write_text(header)
print('Private publisher splash installed: 256x224 / 1700ms / ROM unchanged.')
print('Exact original logo and full-colour publisher card written to assets/.')
PY

# Make the project's ROM/binary safety audit accept exactly two immutable,
# original, explicitly hash-checked branding images; keep all ROM bans intact.
python3 - "$REPO" <<'PY'
from pathlib import Path
import sys
repo=Path(sys.argv[1]);tool=repo/'tools/nxctl.py'
s=tool.read_text()
if 'WYZE_NX_BRANDING_060' not in s:
    insert='''# WYZE_NX_BRANDING_060: only these owned publisher images are permitted.
APPROVED_BRANDING = {
  'assets/wyzecorp-logo.png': 'ab2bfd07fac8a476aac8a7534bbf446113dcb901459bf28a805f0acbd73e85d8',
  'assets/wyzecorp-publisher.png': '5d72a053570e36a31637978231a2390875977f2f1d2f1f890c8c7b986f6cdfa8',
}
def approved_branding(rel, file):
    name=rel.as_posix()
    if name not in APPROVED_BRANDING: return False
    if file.is_symlink() or not file.is_file() or file.stat().st_size > 64000:
        raise GateError(f'Invalid branding asset: {name}')
    if hashlib.sha256(file.read_bytes()).hexdigest() != APPROVED_BRANDING[name]:
        raise GateError(f'Changed branding asset: {name}')
    return True
'''
    needle='class GateError(RuntimeError):\n    pass\n'
    if s.count(needle)!=1: raise SystemExit('Unexpected nxctl.py license/audit structure')
    s=s.replace(needle,needle+'\n'+insert,1)
    needle='''    for rel in tracked:
        if rel.suffix.lower() in BAD_SUFFIX'''
    if s.count(needle)!=1:raise SystemExit('Unexpected source-audit file loop')
    s=s.replace(needle,'''    for rel in tracked:
        if approved_branding(rel, ROOT / rel): continue
        if rel.suffix.lower() in BAD_SUFFIX''',1)
    needle='''            if name.strip() and (path.suffix.lower() in BAD_SUFFIX'''
    if s.count(needle)!=1:raise SystemExit('Unexpected source-audit historical loop')
    s=s.replace(needle,'''            if path.as_posix() in APPROVED_BRANDING: continue
            if name.strip() and (path.suffix.lower() in BAD_SUFFIX''',1)
    tool.write_text(s)
print('Git safety audit narrowly extended for the two WyzeCorp-owned PNGs.')
PY

cat > "$REPO/README.md" <<'MD'
# Lufia II NX

**Lufia II: Rise of the Sinistrals, recompiled for Nintendo Switch.**

Lufia II NX is an independent homebrew project that brings the original Super Nintendo adventure to the Switch as a native application, using [Lufia2SNESRecomp](https://github.com/Cellenseres/Lufia2SNESRecomp) and the [SNESRecomp](https://github.com/RetroPortingToolKit/snesrecomp) framework.

## v0.6.0 — The game runs!

We've moved beyond the controller/audio test app. Lufia II now boots and runs on real Switch hardware, with working game controls, video, and the original soundtrack. The Switch audio backend has been improved to keep music playing smoothly, and the project now includes a WyzeCorp publishing splash.

This is an **early release**: occasional audio stutters remain. Save/load, long-session play, docked mode, and complete game progression still need broader testing. The planned pixel-perfect 720p widescreen presentation is **not implemented or verified yet**; the current build uses the original SNES-style presenter.

## Run it on your Switch

You'll need a homebrew-capable Switch, a locally built NRO, and your own clean **USA, headerless** copy of Lufia II (`.sfc`). Game files aren't supplied here.

Copy the local build to your SD card like this:

```text
switch/
  Lufia-II-NX/
    lufia2nx-native.nro
    lufia2.sfc
    assets/
      ...
```

Launch `lufia2nx-native.nro` from the Homebrew Menu. For best results, use full application-memory mode rather than applet mode. Use the Switch controls to play. The game and its original soundtrack run locally; no network account is required.

## Build on Ubuntu

This repository contains the project scripts and original Switch-side glue, **not copyrighted game data or generated game code**.

1. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started), the Switch toolchain, CMake, Ninja, Python 3, Git, and the GitHub CLI if publishing.
2. Provide your own clean USA ROM, kept **outside the repository**.
3. Use `scripts/phase2-native.sh` to prepare a native build, followed by the Switch-specific portability fixes documented in the project's scripts.
4. From the prepared developer environment, use `lufia-nx-release-0.6.0.sh` to rebuild and stage the NRO. The release script must be placed and run in `~/Downloads/shared/Lufia-II-NX`.

The current tested build is based on a private, patched SDL3 3.4.10 source tree. **A fresh-machine, one-command build is not yet verified.** Do not use the older `tools/nxctl.py build-switch` diagnostic command expecting the real game; it builds the original stub-based shell.

## Download and rights

GitHub releases contain **ROM-free source**. The NRO built from a personal ROM is prepared locally, not distributed as a release attachment. Lufia II and its original artwork/music belong to their respective rights holders.

WyzeCorp maintains this Switch adaptation and publishing artwork. Original recompilation work is credited to **Cellenseres**, with underlying contributions from **SNESRecomp** and its contributors. Original code in this repository is MIT licensed; upstream components retain their own licenses, including PolyForm Noncommercial 1.0.0. This project is not affiliated with or endorsed by Nintendo or the game's rights holders.
MD
cat > "$REPO/docs/RELEASE_NOTES.md" <<'MD'
# Lufia II NX — v0.6.0

Our first native Lufia II build has been tested running on a Nintendo Switch.

**What works:** the game boots, gameplay responds to Switch controls, the original soundtrack plays, and the new queue-ahead audio implementation prevents the continuous music distortion reported in earlier builds. This release also adds a WyzeCorp publisher screen before the game.

**Known limitations:** occasional music stutter; save/load and lengthy gameplay not comprehensively validated; the intended true 720p widescreen and pixel-perfect presentation are still under development.

**For developers:** this GitHub release includes source only. Bring your own clean USA `.sfc` and build locally using the supported dev environment. No ROM, generated ROM code, or game executable is attached to this public release.

Thanks to Cellenseres, SNESRecomp, SDL3, devkitPro, and their respective contributors. Not affiliated with Nintendo or the original game rights holders.
MD
printf '0.6.0\n' > "$REPO/VERSION"
# Keep the publisher card builder available for future local maintenance.
# Publish the EXACT audited script currently running, not a stale copy.
# Important for shared/Windows-backed Downloads folders where sed -i can fail
# with "Text file busy" because it tries to rename an open script.
cp -- "$SCRIPT_DIR/$(basename -- "${BASH_SOURCE[0]}")" "$REPO/scripts/release-0.6.0.sh"
chmod 0755 "$REPO/scripts/release-0.6.0.sh"

note 'Building current private game and validating AArch64 output...'
if ! (set -o pipefail; cmake --build "$BUILD" --target Lufia2Recomp --parallel "${JOBS:-4}" 2>&1 | tee "$LOG"); then
   grep -nE 'FAILED:|fatal error:|undefined reference|error:' "$LOG" | tail -30 >&2 || true
   fail "Build failed. GitHub is unchanged. Build log: $LOG"
fi
ELF="$BUILD/Lufia2Recomp.elf"
[[ -s "$ELF" ]] || fail 'No native game ELF after build'
aarch64-none-elf-readelf -h "$ELF" | grep -q 'Machine:.*AArch64' || fail 'Not a Switch AArch64 executable'
TMP="$(mktemp -d "$ART/.release-060.XXXXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT
nacptool --create 'Lufia II NX' 'WyzeCorp' "$VERSION" "$TMP/game.nacp"
elf2nro "$ELF" "$TMP/game.nro" --nacp="$TMP/game.nacp"
python3 - "$TMP/game.nro" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);data=p.read_bytes()
if len(data)<4096 or data[0x10:0x14]!=b'NRO0':
    raise SystemExit('Invalid NRO package; GitHub unchanged')
print('Switch NRO validated:',len(data),'bytes')
PY
# Preserve the last known-good NRO until a validated replacement exists.
if [[ -f "$STAGE/lufia2nx-native.nro" ]]; then
  cp -p "$STAGE/lufia2nx-native.nro" "$ART/lufia2nx-before-v060.nro"
fi
cp "$TMP/game.nro" "$STAGE/.lufia2nx-native.nro.next"
mv -f "$STAGE/.lufia2nx-native.nro.next" "$STAGE/lufia2nx-native.nro"
cp "$TMP/game.nro" "$ART/lufia2nx-v0.6.0-private.nro"
sha256sum "$STAGE/lufia2nx-native.nro"
[[ -d "$STAGE/assets" ]] || fail 'Private NRO built, but staged runtime assets are missing; GitHub unchanged.'
[[ -f "$STAGE/lufia2.sfc" ]] || fail 'Private NRO built, but staged ROM is missing; GitHub unchanged.'
# Verify stage ROM is the approved USA variant; never copy it into Git.
python3 - "$STAGE/lufia2.sfc" <<'PY'
import hashlib,sys
p=sys.argv[1]
if hashlib.sha256(open(p,'rb').read()).hexdigest() != '7c34ecb16c10f551120ed7b86cfbc947042f479b52ee74bb3c40e92fdd192b3a':
    raise SystemExit('Private SD staging ROM checksum mismatch. GitHub unchanged.')
PY

note 'Auditing source files before GitHub publishing...'
# Only two exact PNG assets are exempted from the normal no-binaries rule.
git -C "$REPO" add -- README.md docs/RELEASE_NOTES.md VERSION tools/nxctl.py scripts/release-0.6.0.sh scripts/phase2-native.sh
git -C "$REPO" add -f -- assets/wyzecorp-logo.png assets/wyzecorp-publisher.png
(
  cd "$REPO"
  git diff --cached --check
  python3 -m unittest discover -s tests -v
  python3 tools/nxctl.py audit
)
python3 - "$REPO" <<'PY'
import subprocess,sys
r=sys.argv[1]
paths=subprocess.check_output(['git','-C',r,'diff','--cached','--name-only','-z']).decode().split('\0')
allowed={'README.md','docs/RELEASE_NOTES.md','VERSION','tools/nxctl.py',
         'scripts/release-0.6.0.sh','scripts/phase2-native.sh','assets/wyzecorp-logo.png','assets/wyzecorp-publisher.png'}
actual={p for p in paths if p}
if not actual or not actual <= allowed:raise SystemExit('Unexpected staged files: '+str(actual-allowed))
print('Publishable source files audited:',len(actual))
PY

note 'Checking GitHub visibility and publishing v0.6.0...'
private="$(gh repo view "$REPO_NAME" --json isPrivate --jq .isPrivate)"
if [[ "$private" == true ]]; then
  note 'Making the audited ROM-free source repository public, as requested for a shareable release.'
  gh repo edit "$REPO_NAME" --visibility public --accept-visibility-change-consequences ||
      fail 'Could not make the repository public. Nothing pushed; update visibility in GitHub and rerun.'
fi
branch="$(git -C "$REPO" branch --show-current)"
[[ -n "$branch" ]] || fail 'Detached HEAD; no release published'
# Fail rather than overwrite a new remote head or create a divergent release.
git -C "$REPO" fetch origin "$branch:refs/remotes/origin/$branch" --quiet
git -C "$REPO" merge-base --is-ancestor "origin/$branch" HEAD || fail 'Remote contains commits not in local branch; manually reconcile before release.'

# Commit and publish the source. A failure after the push may require recovery,
# but a ROM/NRO is never part of the GitHub payload.
git -C "$REPO" commit -m 'Release v0.6.0 - native Switch port and WyzeCorp publishing screen'
git -C "$REPO" push origin "$branch"
git -C "$REPO" tag "$TAG"
git -C "$REPO" push origin "$TAG"
gh release create "$TAG" --repo "$REPO_NAME" \
   --title 'Lufia II NX v0.6.0 — First Native Switch Build' \
   --notes-file "$REPO/docs/RELEASE_NOTES.md" --prerelease --verify-tag
note 'DONE: ROM-free GitHub release is live, and your playable NRO is staged privately.'
printf 'Release: https://github.com/%s/releases/tag/%s\n' "$REPO_NAME" "$TAG"
printf 'Personal Switch package: %s\n' "$STAGE"
printf 'No copyrighted ROM or ROM-derived game executable was uploaded.\n'
