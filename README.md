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
