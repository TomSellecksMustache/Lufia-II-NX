# SiNXstrals — standalone

Native Lufia II for Nintendo Switch. **Version 0.1.0 — development branch.**

This branch is building a ROM-independent Switch application. The player will eventually download the NRO and add their own `lufia2.sfc` to `switch/SiNXstrals/`.

The standalone execution backend is **not yet implemented**. No playable NRO is available from this branch. It does not use Snes9x or libretro.

## Build on Ubuntu

Install devkitPro (devkitA64, libnx), CMake, Ninja and Python 3; then run:

```bash
cd ~/src/Lufia-II-NX
bash ./scripts/standalone.sh check
bash ./scripts/standalone.sh build
```

## Docker

```bash
cd ~/src/Lufia-II-NX
docker build -f docker/Dockerfile -t sinxstrals-builder .
docker run --rm --user "$(id -u):$(id -g)" \
  -e HOME="$HOME" -v "$HOME:$HOME" -w "$PWD" \
  sinxstrals-builder bash ./scripts/standalone.sh build
```

Both commands are intentionally blocked until the native ROM-independent runtime exists and passes verification. A ROM is not included.
