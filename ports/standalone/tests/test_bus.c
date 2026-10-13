/* SINXSTRALS_BACKEND_010: synthetic bytes only. No commercial game data. */
#include "sinx_bus.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    SinxBus *bus=calloc(1,sizeof *bus);
    assert(bus);
    uint8_t *rom=calloc(32768,1);
    assert(rom);
    rom[0x7fd5]=0x20; rom[0x7ffc]=0x00; rom[0x7ffd]=0x80;
    rom[0]=0xa9;
    assert(sinx_load_rom_bytes(bus,rom,32768)==0);
    assert(sinx_bus_read(bus,0x008000)==0xa9);
    assert(sinx_bus_read(bus,0x808000)==0xa9);
    assert(sinx_bus_read(bus,0x00fffc)==0x00);
    assert(sinx_bus_read(bus,0x00fffd)==0x80);
    sinx_bus_write(bus,0x7e0012,0x34);
    assert(sinx_bus_read(bus,0x000012)==0x34);
    assert(sinx_bus_read(bus,0x800012)==0x34);
    sinx_bus_write(bus,0x7f0000,0x56);
    assert(sinx_bus_read(bus,0x7f0000)==0x56);
    sinx_bus_write(bus,0x700012,0x67);
    assert(sinx_bus_read(bus,0xf00012)==0x67);
    sinx_bus_write(bus,0x002100,0x0f);
    assert(bus->io_writes==1 && bus->last_io_address==0x2100);
    (void)sinx_bus_read(bus,0x004210);
    assert(bus->io_reads==1 && bus->last_io_address==0x4210);
    /* Reject copier headers and wrong mapper; no accidental 512B strip. */
    assert(sinx_load_rom_bytes(bus,rom,32767)==-1);
    rom[0x7fd5]=0x21;
    assert(sinx_load_rom_bytes(bus,rom,32768)==-1);
    sinx_unload_rom(bus); free(bus); free(rom);
    puts("PASS: LoROM reset vector, ROM mirrors, WRAM, SRAM, I/O traps, validation");
    return 0;
}
