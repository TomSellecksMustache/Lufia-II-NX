# SiNXstrals Standalone v0.5.2 — prerelease

- Fixed the 4:3-only 720p image: **16:9 Fill** now uses the entire output, disabling SDL3's logical-presentation letterbox and resetting the viewport/clipping.
- Added `VIDEO_052` diagnostic lines showing actual output size, target rectangle, and 16:9/filter selection.
- Preserved v0.5.1 quick-save replacement, Save/Load confirmations, Nintendo A/B controls, R+R3 shader toggle, existing HD-2D shaders, and audio telemetry.
- Includes the new SiNXstrals icon.

**Important:** full width currently stretches the SNES picture; it does not expand in-game field of view. Audio slowdown still needs hardware diagnosis. Supply your own ROM.
