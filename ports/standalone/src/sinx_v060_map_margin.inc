/* SINX_V060_WORLD_SHADOW: ROM-free map-aware widescreen margins.
 * Use the full processed tile map in WRAM bank $7F, not the 32-column PPU VRAM
 * streaming ring. Only enable widened maps after proving the WRAM source
 * agrees with the visible SNES viewport. Never modify guest VRAM/SRAM.
 */
#include "snes/ws_shadow.h"
#define SINX_V060_MARGIN_MARKER "VIDEO_060_NO_WRAP"
#define SINX_V060_WIDE_EXTRA 43
#define SINX_V060_BIAS 1024
#define SINX_V060_MAX_CELLS 16384
#define SINX_V060_ROOM_BORDER 0x10

typedef struct SinxV060Map {
    const uint8_t *data, *blocks;
    uint32_t cells;
    uint16_t block_count;
    uint8_t width, height;
} SinxV060Map;
static bool sinx_v060_shadow_active;
static uint64_t sinx_v060_map_signature;
static uint8_t sinx_v060_last_status = 255;
static uint64_t sinx_v060_frame;
static uint16_t sinx_v060_read16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static void sinx_v060_disarm(void) {
    if (sinx_v060_shadow_active) WsShadowReset();
    sinx_v060_shadow_active = false;
}
static bool sinx_v060_read_map(SinxV060Map *m) {
    const uint8_t *bank = g_ram + 0x10000;
    if (bank[0] != 0x02 || !bank[8] || !bank[9]) return false;
    uint32_t cells = (uint32_t)bank[8] * bank[9];
    uint32_t start = 10u + 4u + cells * 4u;
    if (cells > SINX_V060_MAX_CELLS || start+16 >= 0x10000u) return false;
    uint32_t end = start + 8192u;
    if (end > 0x10000u - 16u) end = 0x10000u - 16u;
    for (uint32_t at=start; at<=end; at+=2) {
        if (bank[at]!='M' || bank[at+1]!='C') continue;
        uint16_t n=sinx_v060_read16(bank+at+2);
        if (!n || at+16u+(uint32_t)n*8u > 0x10000u) continue;
        m->data=bank; m->blocks=bank+at;
        m->cells=cells; m->block_count=n;
        m->width=bank[8]; m->height=bank[9];
        return true;
    }
    return false;
}
static bool sinx_v060_resolve(const SinxV060Map *m,int layer,
                              int32_t tx,int32_t ty,uint16_t *out) {
    int32_t x=tx >> 1, y=ty >> 1;
    if (x<0 || y<0 || x>=m->width || y>=m->height) return false;
    uint32_t cell=(uint32_t)y*m->width+(uint32_t)x;
    uint32_t offset=10u+(layer ? (m->cells*2u+4u) : 0u)+2u*cell;
    uint16_t block=(uint16_t)(m->data[offset] | ((m->data[offset+1]&3u)<<8));
    if (block>=m->block_count) return false;
    unsigned sub=((unsigned)(tx&1)<<1) | (unsigned)(ty&1);
    *out=sinx_v060_read16(m->blocks+16u+(uint32_t)block*8u+sub*2u);
    return true;
}
static int32_t sinx_v060_unwrap(uint32_t scroll,int32_t near) {
    int32_t d=(int32_t)((scroll-(uint32_t)near)&1023u);
    if(d>=512) d-=1024;
    return near+d;
}
static void sinx_v060_camera(const SinxV060Map *m,
                            int32_t *x,int32_t *y) {
    uint16_t px=sinx_v060_read16(g_ram+0x1ddae);
    uint16_t py=sinx_v060_read16(g_ram+0x1de3e);
    if ((unsigned)px/16u>=m->width || (unsigned)py/16u>=m->height) {
        *x=(int32_t)g_ppu->hScroll[0];
        *y=(int32_t)g_ppu->vScroll[0];
        return;
    }
    *x=sinx_v060_unwrap(g_ppu->hScroll[0],(int32_t)px-128);
    *y=sinx_v060_unwrap(g_ppu->vScroll[0],(int32_t)py-112);
}
static bool sinx_v060_native_match(const SinxV060Map *m,
                                   int32_t cx,int32_t cy) {
    unsigned checked=0,matched=0;
    for(int bg=0;bg<2;bg++) {
        int layer=bg==0?1:0;
        uint16_t base=(uint16_t)PPU_bgTilemapAdr(g_ppu,bg);
        int32_t x0=(cx+(int32_t)g_ppu->hScroll[bg]-(int32_t)g_ppu->hScroll[0])>>3;
        int32_t y0=(cy+(int32_t)g_ppu->vScroll[bg]-(int32_t)g_ppu->vScroll[0])>>3;
        for(int dy=0;dy<8;dy++) for(int dx=0;dx<16;dx++) {
            int32_t tx=x0+dx,ty=y0+dy;
            uint16_t expected;
            if (!sinx_v060_resolve(m,layer,tx,ty,&expected)) continue;
            uint16_t address=(uint16_t)(base+(((uint32_t)ty&31u)<<5)+((uint32_t)tx&31u));
            checked++;
            if (g_ppu->vram[address&0x7fffu]==expected) matched++;
        }
    }
    return checked>=64 && matched*100u>=checked*90u;
}
static bool sinx_v060_has_borders(const SinxV060Map *m) {
    /* Native's inferred-room policy refuses all maps with author-defined
       room borders: extending them can expose an unrelated interior. */
    for(uint32_t cell=0;cell<m->cells;cell++) {
        uint32_t at=10u+m->cells*2u+4u+cell*2u;
        if (m->data[at+1]&SINX_V060_ROOM_BORDER) return true;
    }
    return false;
}
/* Call AFTER the frame policy chooses regular-map widening but BEFORE
 * PpuBeginDrawing. The shadow is world-keyed and only changes the two
 * offscreen gutters; central guest hardware rendering stays intact. */
static bool sinx_v060_prepare_map(void) {
    SinxV060Map m={0};
    if(g_ram[0x05ac]==0 || !sinx_v060_read_map(&m)) {
        sinx_v060_disarm(); return false;
    }
    const uint64_t signature=(uint64_t)g_ram[0x05ac] |
       ((uint64_t)m.width<<8) | ((uint64_t)m.height<<16) |
       ((uint64_t)m.block_count<<24) |
       ((uint64_t)(uint32_t)(m.blocks-m.data)<<40);
    int32_t cx,cy;
    sinx_v060_camera(&m,&cx,&cy);
    if (!sinx_v060_shadow_active || sinx_v060_map_signature!=signature) {
        if(sinx_v060_has_borders(&m) || !sinx_v060_native_match(&m,cx,cy)) {
            sinx_v060_disarm(); return false;
        }
        WsShadowReset();
        sinx_v060_map_signature=signature;
        sinx_v060_shadow_active=true;
    }
    for(int bg=0;bg<2;bg++) {
        WsShadowSetWorld(bg,(uint32_t)g_ppu->hScroll[bg]+SINX_V060_BIAS,
                              (uint32_t)g_ppu->vScroll[bg]);
        WsShadowSetScroll(bg,g_ppu->hScroll[bg],g_ppu->vScroll[bg]);
        WsShadowSetBlankTile(bg,0);
    }
    PpuSetExtraSpace(g_ppu,SINX_V060_WIDE_EXTRA);
    PpuSetWidescreenLayerMask(g_ppu,0x03u); /* Map BG1/BG2 only; keep UI BG3 centered. */
    PpuSetWidescreenLayerRepeat(g_ppu,0x00u); /* Never repeat live scene edges. */
    PpuSetWidescreenWindowExpansion(g_ppu,0x33u,0x03u);
    WsShadowFrame(g_ppu);
    for(int bg=0;bg<2;bg++) {
        int logical=bg==0?1:0;
        uint32_t origin=(uint32_t)g_ppu->hScroll[bg]+SINX_V060_BIAS;
        uint32_t sy=g_ppu->vScroll[bg];
        int32_t dx=(cx+(int32_t)g_ppu->hScroll[bg]-(int32_t)g_ppu->hScroll[0] -
                   (int32_t)origin)>>3;
        int32_t dy=(cy+(int32_t)g_ppu->vScroll[bg]-(int32_t)g_ppu->vScroll[0] -
                   (int32_t)sy)>>3;
        uint32_t reach=SINX_V060_WIDE_EXTRA+8u;
        uint32_t x0=(origin-reach)>>3;
        uint32_t x1=(origin+256u+reach+7u)>>3;
        uint32_t y0=(sy>8u?sy-8u:0u)>>3;
        uint32_t y1=(sy+224u+15u)>>3;
        for(uint32_t ty=y0;ty<y1;ty++) for(uint32_t tx=x0;tx<x1;tx++) {
            uint16_t tile=0;
            (void)sinx_v060_resolve(&m,logical,(int32_t)tx+dx,(int32_t)ty+dy,&tile);
            WsShadowForceTile(bg,tx,ty,tile);
        }
    }
    return true;
}
