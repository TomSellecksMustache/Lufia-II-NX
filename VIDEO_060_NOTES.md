# SiNXstrals Standalone 0.6.0 — world-keyed widescreen margins

- 342x224 SNES source frame presented at 1280x720.
- Regular-map gutters reconstruct **actual adjacent tiles** from Lufia II's
  processed WRAM map via SNESRecomp's shadow renderer, after a strict
  native-viewport consistency test. The opposite-edge wrap is not reused.
- Unknown or bounded interiors, menus, fades and unsupported scenes are
  presented centered rather than repeating graphics across the opposite edge.
- Native Mode 7 overworld retains wide rendering.
- Standalone CPU/APU, audio pipeline, menu and save-state format unchanged.

**Experimental:** complex authored room boundaries and screen transitions are
not fully ported; an unsupported area intentionally falls back to 4:3.
No ROM, save data, or ROM-derived compiled game instructions are included.
