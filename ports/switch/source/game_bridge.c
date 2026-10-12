/* Unimplemented on purpose. Do not label the diagnostic shell a playable port. */
#include "game_bridge.h"
bool nxgame_init(const char *rom_path) { (void)rom_path; return false; }
void nxgame_input(uint16_t keys) { (void)keys; }
void nxgame_step(void) {}
const uint32_t *nxgame_frame(int *w, int *h) { *w = *h = 0; return NULL; }
size_t nxgame_audio(int16_t *out, size_t n, int rate) { (void)out; (void)n; (void)rate; return 0; }
bool nxgame_save(void) { return false; }
void nxgame_close(void) {}
