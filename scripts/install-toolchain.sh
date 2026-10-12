#!/usr/bin/env bash
set -Eeuo pipefail
# No silent remote script execution: download for user's inspection first.
sudo apt-get update
sudo apt-get install -y git gh curl wget ca-certificates build-essential make cmake ninja-build python3 pkg-config \
  libgl1-mesa-dev libegl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxfixes-dev libxi-dev libxss-dev libxkbcommon-dev libwayland-dev wayland-protocols \
  libdecor-0-dev libdrm-dev libgbm-dev libasound2-dev libpulse-dev libudev-dev libdbus-1-dev
if ! command -v dkp-pacman >/dev/null 2>&1; then
  echo 'devkitPro pacman is not installed. Official install script will be downloaded to:'
  echo '/tmp/install-devkitpro-pacman'
  curl --fail --location --output /tmp/install-devkitpro-pacman https://apt.devkitpro.org/install-devkitpro-pacman
  chmod +x /tmp/install-devkitpro-pacman
  echo 'Inspect it and run: sudo /tmp/install-devkitpro-pacman'
  echo 'Then run this script again.'
  exit 2
fi
sudo dkp-pacman -Syu --noconfirm
sudo dkp-pacman -S --needed --noconfirm switch-dev switch-sdl2
cat <<'NOTICE'
Export these in your shell, or add to ~/.bashrc:
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=$DEVKITPRO/devkitA64
export PATH=$DEVKITPRO/portlibs/switch/bin:$DEVKITPRO/tools/bin:$DEVKITA64/bin:$PATH
NOTICE
