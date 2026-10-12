# SiNXstrals

**Native Lufia II for Nintendo Switch.** An independent homebrew port.

## Build on Ubuntu

Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) (devkitA64 and libnx), CMake, Ninja, Python 3, and Git. Provide your own legally obtained, compatible Lufia II ROM.

The native build currently needs a **locally prepared private recompilation workspace** at `~/.cache/lufia2nx/`. ROMs and generated game code are not included in this repository.

```bash
cd ~/src/Lufia-II-NX
bash ./scripts/build-private.sh
```

The NRO is staged at `~/Downloads/shared/Lufia-II-NX/switch/lufia2nx-native.nro`.

## Build with Docker

Requires Docker and the **same locally prepared private workspace**. Docker provides the toolchain; it does not generate copyrighted game sources from this public repository.

```bash
cd ~/src/Lufia-II-NX
docker build -f docker/Dockerfile -t sinxstrals-builder .
docker run --rm --user "$(id -u):$(id -g)" \
  -e HOME="$HOME" -v "$HOME:$HOME" -w "$PWD" \
  sinxstrals-builder bash ./scripts/build-private.sh
```

The private Switch executable is built locally and is **not provided as a GitHub download**. This repository and its release ZIP contain source and build scripts only. The standalone ROM-independent application is being developed on the [`standalone`](../../tree/standalone) branch.

SiNXstrals is maintained by WyzeCorp. Original recompilation work: [Cellenseres](https://github.com/Cellenseres/Lufia2SNESRecomp) and the [SNESRecomp](https://github.com/RetroPortingToolKit/snesrecomp) contributors. Upstream licenses apply; this project is not affiliated with Nintendo or the original rights holders.
