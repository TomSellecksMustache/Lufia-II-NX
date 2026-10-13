# SiNXstrals standalone 0.1.0 — backend checkpoint

This branch has an original cartridge loader and 24-bit bus bridge to
SNESRecomp's MIT-origin LakeSnes `interp816` interpreter. Host tests use
synthetic test cartridge bytes only; no game ROM or ROM-derived code ships.

Validated by `sinxstrals-standalone-backend-010.sh build` on Ubuntu:
- LoROM reset vector and cartridge bank reads
- 128 KiB working RAM, mirrors and 32 KiB prototype SRAM window
- Real 65816 opcode stepping: `LDA #$2A; STA $0000; STP`
- Optional `SINX_ROM=/path/to/lufia2.sfc` bounded CPU probe; no ROM copied

**Not implemented yet:** correct non-power-of-two ROM mirroring, functional
PPU/APU/DMA/joypad registers, cycle/scanline scheduling, interrupts,
saving/loading SRAM on disk, Switch NRO packaging and actual gameplay.
The public release gate must remain CLOSED. Do not commit test ROMs.

Upcoming: connect the interpreter to the existing independently licensed
SNESRecomp device bus and event scheduler; then start the Nintendo Switch
frontend, and verify the ROM on hardware. SNESRecomp is PolyForm
Noncommercial; original interp816 derives from LakeSnes under MIT.
