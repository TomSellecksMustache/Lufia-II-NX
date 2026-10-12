# Standalone implementation plan — 0.1.0

- Replace the generated-from-ROM native instruction output with a ROM-independent 65816 execution backend integrated directly into SiNXstrals.
- Reuse independently licensed SNES hardware/runtime modules where permitted. Do not introduce Snes9x or a libretro frontend.
- Load the user's `sdmc:/switch/SiNXstrals/lufia2.sfc` at runtime; validate ROM size and mapping.
- Integrate controls, video, audio, SRAM saves and the original WyzeCorp publisher artwork.
- Restore real widescreen and GPU post-processing where compatible.
- Benchmark and optimize the resulting runtime, then package a ROM-free NRO and SD-card ZIP.
- Gate releases on a clean compiler/linker provenance audit and physical Switch gameplay/save/load testing.

This branch is a source-only work-in-progress, not a playable release.
