/* SINXSTRALS_BACKEND_010: original cartridge and bus integration, no ROM bytes. */
#ifndef SINX_BUS_H
#define SINX_BUS_H
#include <stddef.h>
#include <stdint.h>
typedef struct SinxBus {
    uint8_t *rom;
    size_t rom_size;
    uint8_t wram[128 * 1024];
    uint8_t sram[32 * 1024];
    uint32_t io_reads, io_writes;
    uint16_t last_io_address;
} SinxBus;
/* Supports headerless LoROM only. Returns 0 on success, -1 on failure. */
int sinx_load_rom(SinxBus *bus, const char *path);
int sinx_load_rom_bytes(SinxBus *bus, const uint8_t *src, size_t size);
void sinx_unload_rom(SinxBus *bus);
/* Callbacks deliberately match interp816's memory bus signatures. */
uint8_t sinx_bus_read(void *ctx, uint32_t address);
void sinx_bus_write(void *ctx, uint32_t address, uint8_t value);
#endif
