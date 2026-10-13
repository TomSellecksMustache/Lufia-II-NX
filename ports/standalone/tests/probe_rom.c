/* SINXSTRALS_BACKEND_010: optional bounded runtime probe of user-owned ROM. */
#include "sinx_bus.h"
#include "interp816.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr,"Usage: probe_rom /path/to/your/lufia2.sfc\n"); return 2; }
    SinxBus *bus=calloc(1,sizeof *bus);
    if (!bus || sinx_load_rom(bus,argv[1])) {
        fprintf(stderr,"ROM rejected: require a headerless LoROM file with valid reset vector\n");
        free(bus); return 1;
    }
    Interp816 *cpu=interp816_init(bus,sinx_bus_read,sinx_bus_write);
    if (!cpu) { sinx_unload_rom(bus); free(bus); return 1; }
    interp816_set_brk_hook_enabled(cpu,false);
    interp816_reset(cpu);
    unsigned reset=cpu->pc, n=0;
    unsigned long cycles=0;
    for (; n<100000 && !cpu->stopped; ++n) {
        if (bus->io_reads || bus->io_writes) break;
        cycles+=(unsigned)interp816_runOpcode(cpu);
    }
    printf("ROM accepted: %zu bytes, reset=$%04X, instructions=%u, cycles=%lu\n",
           bus->rom_size,reset,n,cycles);
    if (bus->io_reads || bus->io_writes)
        printf("First unimplemented hardware register: $%04X (needs real PPU/APU/CPU IO)\n",bus->last_io_address);
    else
        printf("Bounded probe stopped without I/O; game is NOT considered booted.\n");
    interp816_free(cpu); sinx_unload_rom(bus); free(bus);
    return 0;
}
