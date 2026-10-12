# Engineering guide: crossing from the Switch shell to the real game

## Critical current status

The checked-in `ports/switch/source/game_bridge.c` intentionally implements a
NO-OP interface. It never executes Lufia II. `0.6.0` or higher must not be
tagged until the following engine work is completed and physically verified.

## Codebases to study (local clones only)

- `https://github.com/Cellenseres/Lufia2SNESRecomp` — gameplay host, patches, native
  recompiler configuration and CMake generation. Its `ports/vita/main.c` and
  `ports/vita/native.cmake` establish an existing console host approach.
- `https://github.com/Cellenseres/snesrecomp-platform` — runtime policy and native
  presenter adaptation; inspect the actual headers of the checked-out commit.
- `https://github.com/Cellenseres/Lufia2Decomp` — semantics of selected functions.

Upstream currently expects SDL3 and desktop/Vita integrations. The included
ROM-free Switch diagnostic deliberately uses devkitPro's SDL2 port instead of
assuming an official SDL3 for homebrew. The engine MUST NOT merely link SDL3
and SDL2 into one binary. Port the host-facing implementation at the API seam.

## `nxgame_*` obligations

The build and test interface lives at `ports/switch/include/game_bridge.h`:

| Function | Real responsibility |
|---|---|
| `nxgame_init` | Open supplied `sdmc:` ROM, SHA-256 validate, create guest memory and boot without embedded ROM blobs |
| `nxgame_input` | Receive standard SNES 12-bit pad state at game cadence, support full D-pad and face buttons |
| `nxgame_step` | Drive one simulated SNES frame/PPU refresh at intended rate with correct runtime/IRQ semantics |
| `nxgame_frame` | Return ARGB8888 pixels owned by game core, width/height valid until next frame |
| `nxgame_audio` | Fill 48-kHz stereo signed 16-bit buffer, thread-safe with stepping and teardown |
| `nxgame_save` | Atomically persist battery-backed RAM to user-owned SD path |
| `nxgame_close` | Free native/guest resources without letting audio callback race with teardown |

The shell currently maps A/B/X/Y/L/R and the D-pad, Plus to Start, Minus to
Select, and Plus+Minus together to exit. Those are *skeleton mappings* and
should be verified in gameplay, menus, and battles. SDL scaling is currently a
16:9 centered 4:3 content transform for guest frames; HD Mode7, widescreen
maps, shader filters, frame pacing, performance tests and overlays require
actual PPU integration. The screen advertised at <=0.5 is **not** a game frame.

## Performance and fidelity milestones

1. Reproduce `src/main.c` boot and frame lifecycle using an SDL-free core.
2. Reuse the game's generated dispatch and original runtime in the *local*
   build tree, not by copying generated ROM-derived C into this public repo.
3. Determine actual build requirements of the pinned SNESrecomp core for ARM64
   (includes, CPU/JIT assumptions, endianness, page/thread stacks, memory).
4. Replace desktop/Vita presentation with SDL2 texture upload to the Switch;
   use the visible `nxgame_frame` interface and proper aspect ratio.
5. Initialize original sound and lock/synchronize audio with game logic.
6. Load original US ROM from SD at runtime; provide useful on-screen errors for
   missing/wrong ROM and never duplicate the ROM to GitHub build artifacts.
7. Confirm input, PPU/Mode7, battle effects, SRAM persistence, framerate,
   dock/undock behavior, home menu suspend/resume, and exit recovery.
8. Only after stable basics, expose optional widescreen, quality-of-life, and
   visual improvements; no mandatory heavy shaders or touch controls.

## Dependency and copyright boundary

Our published GitHub repo contains only independently written source and
configurations. Upstream sources remain in `~/.cache/lufia2nx/upstream/`; code
derived from ROM by the native recompiler goes into a cache build directory.
MIT does not erase the upstream PolyForm Noncommercial restrictions. Do NOT
attach a game executable containing recompiled copyrighted material to a
public release without confirming the applicable distribution rights.

## Testing contract

You may run `python3 tools/nxctl.py milestone 0.6.0 --publish`, but its first
step is to inspect `game_bridge.c` and stop when the starter stub remains.
Replacing the stub alone is not sufficient: the runner then requires explicit
real-hardware confirmation. Passing `0.7.0` requires functioning battle,
dungeon, save/reload and non-touch navigation. `0.8.0` requires extended
handheld+docked testing. The tool does not fake any results.
