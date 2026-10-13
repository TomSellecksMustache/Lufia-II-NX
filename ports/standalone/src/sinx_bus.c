/* SINXSTRALS_BACKEND_010: independent 24-bit LoROM/WRAM/SRAM bus skeleton.
 * PPU/APU/CPU I/O is explicitly unimplemented; it MUST be connected before gameplay.
 * ROM mirroring uses modulo for this early smoke-test, not full hardware mirroring. */
#include "sinx_bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sinx_load_rom_bytes(SinxBus *b, const uint8_t *src, size_t size) {
    if (!b || !src || size < 32768 || size > 8u*1024u*1024u || size % 32768)
        return -1;
    /* 0x20 = LoROM, 0x30 = Fast LoROM. Other mappers unsupported. */
    if (src[0x7fd5] != 0x20 && src[0x7fd5] != 0x30) return -1;
    if (((unsigned)src[0x7ffd] << 8 | src[0x7ffc]) < 0x8000) return -1;
    uint8_t *copy = malloc(size);
    if (!copy) return -1;
    memcpy(copy, src, size);
    free(b->rom);
    b->rom=copy; b->rom_size=size;
    memset(b->wram, 0, sizeof b->wram);
    memset(b->sram, 0, sizeof b->sram);
    b->io_reads=b->io_writes=0;
    return 0;
}
int sinx_load_rom(SinxBus *b, const char *path) {
    if (!b || !path) return -1;
    FILE *f=fopen(path,"rb");
    if (!f) return -1;
    if (fseek(f,0,SEEK_END)) { fclose(f); return -1; }
    long n=ftell(f);
    if (n<32768 || n>8L*1024*1024 || n%32768 || fseek(f,0,SEEK_SET)) {
        fclose(f); return -1;
    }
    uint8_t *buf=malloc((size_t)n);
    if (!buf) { fclose(f); return -1; }
    int ok=fread(buf,1,(size_t)n,f)==(size_t)n;
    fclose(f);
    int result=ok ? sinx_load_rom_bytes(b,buf,(size_t)n) : -1;
    free(buf);
    return result;
}
void sinx_unload_rom(SinxBus *b) {
    if (!b) return;
    free(b->rom); b->rom=NULL; b->rom_size=0;
}
uint8_t sinx_bus_read(void *ctx,uint32_t address) {
    SinxBus *b=(SinxBus *)ctx;
    if (!b) return 0xff;
    address &= 0xffffffu;
    unsigned bank=(address>>16)&0xffu, off=address&0xffffu;
    if (bank==0x7e || bank==0x7f)
        return b->wram[((bank-0x7e)<<16)|off];
    unsigned canonical=bank&0x7fu;
    if (canonical<0x40) {
        if (off<0x2000) return b->wram[off];
        if (off<0x6000) {
            ++b->io_reads; b->last_io_address=(uint16_t)off;
            return 0xff; /* Unsupported PPU/APU/CPU registers: fail-open bus */
        }
    }
    if (bank>=0x70 && bank<0x7e && off<0x8000)
        return b->sram[(((bank&15u)<<15)|off)%sizeof b->sram];
    if (bank>=0xf0 && off<0x8000)
        return b->sram[(((bank&15u)<<15)|off)%sizeof b->sram];
    if (b->rom && b->rom_size && (off>=0x8000 || canonical>=0x40)) {
        size_t ix=((size_t)canonical<<15)|(off&0x7fffu);
        return b->rom[ix % b->rom_size];
    }
    return 0xff;
}
void sinx_bus_write(void *ctx,uint32_t address,uint8_t v) {
    SinxBus *b=(SinxBus *)ctx;
    if (!b) return;
    address &= 0xffffffu;
    unsigned bank=(address>>16)&0xffu, off=address&0xffffu;
    if (bank==0x7e || bank==0x7f) {
        b->wram[((bank-0x7e)<<16)|off]=v; return;
    }
    unsigned canonical=bank&0x7fu;
    if (canonical<0x40) {
        if (off<0x2000) { b->wram[off]=v; return; }
        if (off<0x6000) {
            ++b->io_writes; b->last_io_address=(uint16_t)off;
            return; /* I/O dispatch not installed yet */
        }
    }
    if (((bank>=0x70 && bank<0x7e) || bank>=0xf0) && off<0x8000)
        b->sram[(((bank&15u)<<15)|off)%sizeof b->sram]=v;
    /* Writes to cartridge ROM are ignored. */
}
