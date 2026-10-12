#ifndef LUFIA2NX_GAME_BRIDGE_H
#define LUFIA2NX_GAME_BRIDGE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Implement this interface in a private local port of the recompiled runtime.
 * The starter returns false: it is a diagnostic shell, not a game.
 * A bridge must load the ROM at runtime from SD and NEVER embed ROM data. */
bool nxgame_init(const char *rom_path);
void nxgame_input(uint16_t snes_keys);
void nxgame_step(void);
const uint32_t *nxgame_frame(int *width, int *height);
size_t nxgame_audio(int16_t *out, size_t frames, int sample_rate);
bool nxgame_save(void);
void nxgame_close(void);
#endif
