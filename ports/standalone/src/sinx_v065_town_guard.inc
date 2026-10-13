/* SINX_V065_TOWN_GUARD: native-derived temporary town tile/window overlay.
 * Guest PPU/VRAM writes survive; undo only entries still carrying our value.
 * Never retain these VRAM substitutions beyond a reference emulation frame.
 */
#define SINX_V065_NATIVE_WIDTH 256
#define SINX_V065_MAX_RESTORE 512
#define SINX_V065_GUARD 16
#define SINX_V065_WIDE_EXTRA 43
#define SINX_V065_MARKER "VIDEO_065_TOWN_GUARD"
typedef struct SinxV065Backup {
    uint16_t addr, old_value, inserted;
} SinxV065Backup;
static SinxV065Backup sinx_v065_restore[SINX_V065_MAX_RESTORE];
static unsigned sinx_v065_count;
static unsigned sinx_v065_guard_frames;
static bool sinx_v065_window_open;
static bool sinx_v065_mosaic_off;
static uint8_t sinx_v065_window_left, sinx_v065_window_right;
static uint8_t sinx_v065_mosaic_saved;
static uint8_t sinx_v065_seen[0x8000/8];
static uint32_t sinx_v065_guards_logged;

static void sinx_v065_put(uint16_t addr,uint16_t tile) {
    addr &= 0x7fffu;
    if (sinx_v065_seen[addr>>3] & (1u<<(addr&7))) {
        /* Do not let two bands accidentally assign an address twice. */
        return;
    }
    if (sinx_v065_count>=SINX_V065_MAX_RESTORE) return;
    sinx_v065_seen[addr>>3] |= (uint8_t)(1u<<(addr&7));
    SinxV065Backup *r=&sinx_v065_restore[sinx_v065_count++];
    r->addr=addr;
    r->old_value=g_ppu->vram[addr];
    r->inserted=tile;
    g_ppu->vram[addr]=tile;
}
static void sinx_v065_restore_band(const SinxV060Map *map,int bg,int x0,int x1,
                                  int32_t cam_x,int32_t cam_y) {
    if(x0>=x1) return;
    const int logical=(bg==0?1:0);
    const uint32_t sx=g_ppu->hScroll[bg];
    const uint32_t sy=g_ppu->vScroll[bg];
    const uint32_t tx0=(sx+(uint32_t)x0)>>3;
    const uint32_t tx1=(sx+(uint32_t)x1-1u)>>3;
    const uint32_t ty0=sy>>3;
    const uint32_t ty1=(sy+223u)>>3;
    const int32_t dx=(sinx_v060_unwrap(sx,cam_x)-(int32_t)sx)>>3;
    const int32_t dy=(sinx_v060_unwrap(sy,cam_y)-(int32_t)sy)>>3;
    const uint16_t base=(uint16_t)PPU_bgTilemapAdr(g_ppu,bg);
    for(uint32_t ty=ty0;ty<=ty1;ty++) {
        for(uint32_t tx=tx0;tx<=tx1;tx++) {
            uint16_t entry;
            if(!sinx_v060_resolve(map,logical,(int32_t)tx+dx,(int32_t)ty+dy,&entry))
                continue;
            const uint16_t addr=(uint16_t)(base+((ty&31u)<<5)+(tx&31u));
            sinx_v065_put(addr,entry);
        }
    }
}
static void sinx_v065_render_begin(void) {
    sinx_v065_count=0;
    sinx_v065_window_open=false;
    sinx_v065_mosaic_off=false;
    memset(sinx_v065_seen,0,sizeof(sinx_v065_seen));
    if(!g_ppu || !sinx_v060_shadow_active || PPU_mode(g_ppu)!=1) return;
    /* A mosaic of size 1 should be visually inert but produces solid columns
       in the new renderer's offscreen-clamped mosaic path. */
    if(g_ppu->mosaic && PPU_mosaicSize(g_ppu)==1) {
        sinx_v065_mosaic_saved=g_ppu->mosaic;
        g_ppu->mosaic=0;
        sinx_v065_mosaic_off=true;
    }
    const uint8_t left=g_ppu->window1left,right=g_ppu->window1right;
    if(left>right || left>SINX_V065_GUARD || right<255-SINX_V065_GUARD)
        return;
    int affected[2]={0,0};
    for(int bg=0;bg<2;bg++) {
        if(((g_ppu->windowsel>>(bg*4))&3u)==3u) affected[bg]=1;
    }
    if(!affected[0] && !affected[1]) return;
    SinxV060Map map={0};
    if(!sinx_v060_read_map(&map) || sinx_v060_has_borders(&map)) return;
    int32_t cx,cy;
    sinx_v060_camera(&map,&cx,&cy);
    if(!sinx_v060_native_match(&map,cx,cy)) return;
    for(int bg=0;bg<2;bg++) if(affected[bg]) {
        sinx_v065_restore_band(&map,bg,0,(int)left,cx,cy);
        sinx_v065_restore_band(&map,bg,(int)right+1,256,cx,cy);
    }
    if(!sinx_v065_count) return;
    sinx_v065_window_left=left;
    sinx_v065_window_right=right;
    g_ppu->window1left=0;
    g_ppu->window1right=255;
    sinx_v065_window_open=true;
    sinx_v065_guard_frames++;
    if(sinx_v065_guards_logged<4 || (sinx_v065_guard_frames%600)==0) {
        sinx_v051_debug("VIDEO_065_TOWN_GUARD map=%u scroll=%u guard=%u/%u tiles=%u",
          (unsigned)g_ram[0x05ac],(unsigned)g_ppu->hScroll[0],
          (unsigned)left,(unsigned)(255-right),sinx_v065_count);
        sinx_v065_guards_logged++;
    }
}
static void sinx_v065_render_end(void) {
    if(sinx_v065_window_open && g_ppu->window1left==0 && g_ppu->window1right==255) {
        g_ppu->window1left=sinx_v065_window_left;
        g_ppu->window1right=sinx_v065_window_right;
    }
    sinx_v065_window_open=false;
    if(sinx_v065_mosaic_off && g_ppu->mosaic==0)
        g_ppu->mosaic=sinx_v065_mosaic_saved;
    sinx_v065_mosaic_off=false;
    /* Preserve the guest's newer DMA/VRAM writes, if any, made this frame. */
    for(unsigned i=0;i<sinx_v065_count;i++) {
        SinxV065Backup *r=&sinx_v065_restore[i];
        if(g_ppu->vram[r->addr]==r->inserted) g_ppu->vram[r->addr]=r->old_value;
    }
    sinx_v065_count=0;
}
