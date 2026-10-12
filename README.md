# Lufia-II-NX

Native Nintendo Switch homebrew port **in development**. This repository contains
only original MIT-licensed platform code and build orchestration. It intentionally
contains **no original SNES ROM, extracted game assets, recompilation output, or
upstream code**. A user's own clean US `.sfc` is required locally.

> Current included Switch build is a **hardware diagnostic**, NOT a playable
> Lufia II port. The game-core integration is an explicit blocked milestone.
> No claim of a functioning game is made until it is tested on real hardware.

## Bootstrap on Ubuntu

```bash
unzip Lufia-II-NX-starter.zip
cd Lufia-II-NX-starter
./bootstrap.sh --rom "/absolute/path/Lufia II - Rise of the Sinistrals (USA).sfc" --github
cd ~/src/Lufia-II-NX
```

`--github` uses `gh auth login` credentials to create
`TomSellecksMustache/Lufia-II-NX` as **private**. Add `--public` for a public
repository. Omitting `--github` creates only the local project. Script refuses
to overwrite an existing project or remote.

The ROM path and builds live outside this checkout in `~/.cache/lufia2nx/`.
No ROM copy is ever placed in Git or GitHub Actions. Local ROM fingerprint:
SHA-256 `7c34ecb16c10f551120ed7b86cfbc947042f479b52ee74bb3c40e92fdd192b3a`.

## Toolchain

```bash
./scripts/install-toolchain.sh
# If devkitPro isn't installed, inspect then run the downloaded official installer.
# Re-run install-toolchain.sh after that.
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=$DEVKITPRO/devkitA64
export PATH=$DEVKITPRO/portlibs/switch/bin:$DEVKITPRO/tools/bin:$DEVKITA64/bin:$PATH
```

## Iterative release commands

```bash
python3 tools/nxctl.py status
python3 tools/nxctl.py iterate --until 0.5.0
# Same, but push tags + release notes to a pre-created GitHub repository:
python3 tools/nxctl.py iterate --until 0.5.0 --publish --attach-shell
# Resume any single gated milestone:
python3 tools/nxctl.py milestone 0.6.0 --publish
python3 tools/nxctl.py iterate --until 0.8.0 --publish
```

Each milestone runs tests, runs the relevant build/check, asks you to verify on
physical hardware when required, then commits and tags **only if all gates pass**.
It cannot auto-verify real Switch gameplay. Hardware confirmations are local,
not trusted GitHub CI tests. A failed step is resumable; no tag is created.
`--attach-shell` only publishes the ROM-free diagnostic `.nro` at 0.1.0-0.5.0.
**Game binaries and generated game code are never uploaded automatically.**

### Native desktop upstream verification

```bash
python3 tools/nxctl.py build-desktop
python3 tools/nxctl.py run-desktop
```

The upstream repository is cloned into `~/.cache/lufia2nx/upstream/`; `cmake`
regenerates private machine-code-derived C outside this Git repository.

### Install the diagnostic on an SD card

```bash
python3 tools/nxctl.py build-switch
python3 tools/nxctl.py install-sd /media/$USER/SWITCH --copy-rom
```

`--copy-rom` is an **explicit** instruction to copy your original ROM to your
personal Switch SD card, never to Git. Place the SD card in a homebrew-capable
Switch and launch `Lufia-II-NX/lufia2nx.nro` using the Homebrew Menu.
Hold **B** to test sound, test D-pad and ABXY/L/R; press **Plus+Minus** to exit.
If the screen says `GAME CORE NOT INTEGRATED`, this is expected.

## Milestone roadmap

| Version | Deliverable | Gate |
|---|---|---|
| 0.0.1 | Protected repository and legal notices | Static tests and Git audit |
| 0.0.2 | Local ROM verification and isolation | Exact SHA-256 and size |
| 0.1.0 | ROM-free Switch diagnostic build | Real `.nro` compilation |
| 0.2.0 | Native desktop Lufia II build | Upstream CMake build passes |
| 0.3.0 | Controller support | Real Switch button confirmation |
| 0.4.0 | SD ROM discovery | Real SD / app observation |
| 0.5.0 | Video/audio diagnostics | Handheld/docked + audio observation |
| 0.6.0 | Game integration, intro, title, music | Implement `nxgame_*` and test on Switch |
| 0.7.0 | Battle/dungeon/save/reload | Real Switch gameplay regression |
| 0.8.0 | Stable playable build | Extended gameplay / dock tests |

0.6-0.8 require meaningful engineering, not merely script execution. The
`ports/switch/include/game_bridge.h` ABI is designed for an independently
ported game host; `source/game_bridge.c` currently contains intentional stubs.
The upstream **Vita** host at `Cellenseres/Lufia2SNESRecomp/ports/vita` is an
adaptation reference, not a Switch implementation. A robust port needs to wire
frame stepping, output frame, audio callback, SRAM save, ROM verification, and
Switch input to the native engine without bundling copyrighted assets.

## Licensing and rights

All **original starter files** are MIT licensed. Upstream
`Cellenseres/Lufia2SNESRecomp` uses **PolyForm Noncommercial 1.0.0** and has
its own license requirements. `Lufia2Decomp` may have different permissions;
verify upstream and contributor grants. MIT here does **not** relicense upstream
code, game logic, Nintendo assets, or ROM data. Publishing a native binary
containing recompiled original game code raises additional rights questions.

This is not affiliated with Nintendo or any rights holder. Only use ROMs and
hardware you are legally entitled to use.
