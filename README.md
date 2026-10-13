# SiNXstrals — Standalone

[![Standalone release](https://img.shields.io/github/v/release/TomSellecksMustache/SiNXstrals?filter=standalone-v*&label=standalone)](https://github.com/TomSellecksMustache/SiNXstrals/releases)

![SiNXstrals](assets/sinxstrals-v051-icon.jpg)

A standalone Nintendo Switch homebrew runtime for **Lufia II**. Bring your own compatible `lufia2.sfc` ROM. No game files or saves are distributed.

## Install

Get the standalone ZIP from [Releases](https://github.com/TomSellecksMustache/SiNXstrals/releases), extract it to the **root** of your Switch SD card, and copy your ROM to `switch/SiNXstrals/lufia2.sfc`. Launch `switch/SiNXstrals/sinxstrals.nro` from the Homebrew Menu.

## Features

- Hold **−** for two seconds for the in-game menu. **A** confirms; **B** cancels.
- Save State and Load State with confirmation; repeated quick saves replace the previous state.
- Options: **16:9 Fill**, **Pixel Smooth**, **Shaders**. **R + R3** toggles shaders anywhere.
- **16:9 Fill** is on by default. It draws the SNES image across the full 1280×720 screen **by stretching it**, not by rendering additional world outside the original SNES picture. Turning it off gives an aspect-correct 4:3 viewport. Docked output can use the same adaptive rendering path in the future.
- Pixel Smooth changes between linear and nearest-neighbour GPU sampling. Existing HD-2D shader integration is retained.

## Building and status

**v0.5.2 is a hardware-test prerelease.** The published source is a snapshot of a local devkitPro/libnx, SDL3 and SNESRecomp build, not yet a clean-checkout reproducible build. The older `scripts/standalone.sh` is a development placeholder and does not reproduce this release.

Audio slowdown is being investigated, not claimed fixed. Switch log: `sdmc:/switch/SiNXstrals/v051-debug.log` (look for `VIDEO_052` and audio counters). Game ROM not included. Third-party licenses remain with their respective projects.

Unofficial fan project; not affiliated with Nintendo or the original publisher.
