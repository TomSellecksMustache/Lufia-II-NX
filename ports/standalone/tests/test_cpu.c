/* SINXSTRALS_BACKEND_010: real interp816 executing synthetic 65816 machine code.
 * Does not boot Lufia II: SNES video, sound, timing and interrupt IO are next. */
#include "sinx_bus.h"
#include "interp816.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    SinxBus *bus=calloc(1,sizeof *bus);
    uint8_t *rom=calloc(32768,1);
    assert(bus && rom);
    rom[0x7fd5]=0x20; rom[0x7ffc]=0x00; rom[0x7ffd]=0x80;
    /* LDA #$2A; STA $0000; STP */
    const uint8_t program[]={0xa9,0x2a,0x8d,0x00,0x00,0xdb};
    for (unsigned i=0;i<sizeof program;i++) rom[i]=program[i];
    assert(sinx_load_rom_bytes(bus,rom,32768)==0);
    Interp816 *cpu=interp816_init(bus,sinx_bus_read,sinx_bus_write);
    assert(cpu);
    interp816_set_brk_hook_enabled(cpu,false);
    interp816_reset(cpu);
    assert(cpu->pc==0x8000);
    unsigned steps=0,cycles=0;
    while (!cpu->stopped && steps<32) {
        int used=interp816_runOpcode(cpu);
        assert(used>0);
        cycles+=(unsigned)used; steps++;
    }
    assert(cpu->stopped && steps==3 && bus->wram[0]==0x2a);
    printf("PASS: real 65816 interpreter, %u instructions, %u guest cycles, WRAM[0]=$%02X\n",
        steps,cycles,bus->wram[0]);
    interp816_free(cpu); sinx_unload_rom(bus); free(rom); free(bus);
    return 0;
}
