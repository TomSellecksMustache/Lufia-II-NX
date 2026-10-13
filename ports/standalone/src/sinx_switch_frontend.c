/* SINX_V053_WIDE_LAYOUT: scene-aware 342x224 PPU integration */
/* SINX_V051_GUARDED_REPAIR frontend audio probe */
/* SINX_STANDALONE_SWITCH_012: first standalone ROM-at-runtime Switch frontend.
 * Includes the same game-neutral interpreter/hardware reference driver that
 * passed the user's 600-frame Ubuntu test. The old CLI main is not invoked.
 * This file contains NO game ROM or generated game instructions.
 */
#define SDL_MAIN_HANDLED 1
#define main sinx_reference_cli_main
#define run_one_frame sinx_reference_slow_frame
#include "ref_driver.c"
#undef run_one_frame
#undef main
#include <SDL3/SDL.h>


/* SINX_V050_MENU_GPU_SAVE_V1 */
#include "sinx_v050_overlay.inc"
#include "sinx_native_video_policy.c"
/* SINX_V060_WORLD_SHADOW */
#include "sinx_v060_map_margin.inc"
/* SINX_V053_PPU342: native-style 342x224 source width, 43 genuine margin pixels
 * per side. Game-neutral 65816, APU, SRAM, and savestate remain unchanged.
 * The source PPU must be the extended renderer shipped with SNESRecomp.
 * Regular map margin data remains experimental until full native map resolver
 * (VRAM shadow + authored room data) is integrated. */
#define SINX_V053_WIDE_WIDTH 342
#define SINX_V053_WIDE_EXTRA 43
#define SINX_V053_WIDE_PITCH (SINX_V053_WIDE_WIDTH * 4)
static uint8_t sinx_v053_pixels[SINX_V053_WIDE_PITCH * 240];
static Lufia2VideoLayout sinx_v053_last_layout=LUFIA2_VIDEO_LAYOUT_COUNT;
static void sinx_v053_video_begin_frame(void) {
    Lufia2VideoLayout layout =
        Lufia2SelectVideoLayout(g_ppu,sinx_widescreen_fill);
    if(layout==LUFIA2_VIDEO_BLANK &&
       sinx_v053_last_layout!=LUFIA2_VIDEO_LAYOUT_COUNT)
        layout=sinx_v053_last_layout;
    /* Never carry a layer mask from the previous menu into the map. */
    PpuSetWidescreenLayerMask(g_ppu,0);
    PpuSetWidescreenBg3Widen(g_ppu,0);
    bool map_wide=false;
    bool map_selected=sinx_widescreen_fill &&
        layout==LUFIA2_VIDEO_REGULAR_MAP;
    if(map_selected) map_wide=sinx_v060_prepare_map();
    else sinx_v060_disarm();
    if(!sinx_widescreen_fill) {
        PpuSetExtraSpaceCentered(g_ppu,SINX_V053_WIDE_EXTRA);
    } else if(layout==LUFIA2_VIDEO_WORLD_MAP) {
        PpuSetExtraSpace(g_ppu,SINX_V053_WIDE_EXTRA);
        PpuSetWidescreenWindowExpansion(g_ppu,0x31u,0x03u);
    } else if(!map_wide) {
        /* Menus, interiors, loading and unverified maps: centered 4:3.
           No scene fragments may reappear across the opposite gutter. */
        PpuSetExtraSpaceCentered(g_ppu,SINX_V053_WIDE_EXTRA);
    }
    uint8_t status=map_wide?1u:(map_selected?2u:0u);
    if(status!=sinx_v060_last_status || layout!=sinx_v053_last_layout) {
        sinx_v051_debug("VIDEO_060_NO_WRAP layout=%s status=%s width=342 extra=43",
                        Lufia2VideoLayoutName(layout),
                        map_wide?"map-shadow":(map_selected?"safe-fallback":"scene-policy"));
        sinx_v060_last_status=status;
        sinx_v053_last_layout=layout;
    }
    memset(sinx_v053_pixels,0,SINX_V053_WIDE_PITCH*224);
    PpuBeginDrawing(g_ppu,sinx_v053_pixels,SINX_V053_WIDE_PITCH,
                    kPpuRenderFlags_NewRenderer);
}


/* SINX_STANDALONE_VIDEO_RESCUE_V1
 * The Switch frontend owns one streaming texture and one SDL3 renderer.
 * Reset the render TARGET and state before each draw; do not touch emulation,
 * APU, ROM, or timing. A small bright rectangle is shown for 120 draw frames
 * as an independent GPU/display diagnostic (then removed automatically).
 */
#if defined(__SWITCH__)
extern void lufia2_nx_trace(const char *label);
static bool sinx_rescue_upload(SDL_Texture *tex, const SDL_Rect *rect,
                               const void *pixels, int pitch)
{
    static unsigned calls = 0;
    bool ok = SDL_UpdateTexture(tex, rect, pixels, pitch);
    ++calls;
    if (calls == 1)
        lufia2_nx_trace(ok ? "SINX_VIDEO_UPLOAD_OK_V1" : "SINX_VIDEO_UPLOAD_FAILED_V1");
    if (calls == 180)
        lufia2_nx_trace("SINX_VIDEO_UPLOAD_180_FRAMES_V1");
    return ok;
}
static bool sinx_rescue_draw(SDL_Renderer *renderer, SDL_Texture *tex,
                             const SDL_FRect *src, const SDL_FRect *dst)
{
    static unsigned frames = 0;
    ++frames;
    if (frames == 1)
        lufia2_nx_trace("SINX_VIDEO_DRAW_REACHED_V1");
    if (!renderer || !tex) {
        if (frames == 1) lufia2_nx_trace("SINX_VIDEO_NULL_RENDERER_OR_TEXTURE_V1");
        return false;
    }
    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderViewport(renderer, NULL);
    SDL_SetRenderClipRect(renderer, NULL);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    bool ok = sinx_v050_draw(renderer, tex, src, dst);
    if (!ok && frames == 1)
        lufia2_nx_trace("SINX_VIDEO_RENDERTEXTURE_FAILED_V1");
    if (frames == 180)
        lufia2_nx_trace("SINX_VIDEO_DRAW_180_FRAMES_V1");
    return ok;
}
#else
#define sinx_rescue_upload SDL_UpdateTexture
#define sinx_rescue_draw SDL_RenderTexture
#endif

/* The reference H/V driver controls master cycles; avoid recomp-only beam nudges. */
int g_interp_apu_driving = 1;

/* SINX_STANDALONE_SWITCH_013_VIDEO_SPEED
 * Ref timing equivalence: horizontal position 0 starts a scanline (PPU/NMI),
 * 1024 runs HDMA, and H/V IRQs fire at the programmed H-timer position.
 * Other two-master-clock steps only advance H/V counters and auto-joy timer.
 * Skip only those idle ticks; keep the original handler for each real event.
 */
static void sinx_fast_advance_beam(Snes *snes, int master) {
    int ticks = master / 2;
    while (ticks > 0) {
        int pos = snes->hPos;
        int irqpos = snes->hIrqEnabled ? 4 * (int)snes->hTimer : -1;
        if (pos == 0 || pos == 1024 || pos == irqpos) {
            handle_pos_stuff(snes);
            ticks--;
            continue;
        }
        int next = pos < 1024 ? 1024 : 1364;
        if (irqpos > pos && irqpos < next) next = irqpos;
        int count = (next - pos) / 2;
        if (count > ticks) count = ticks;
        /* Fallback rather than looping forever if a device ever sets an
         * unexpected odd H-position. */
        if (count <= 0) {
            handle_pos_stuff(snes);
            ticks--;
            continue;
        }
        if (snes->autoJoyTimer) {
            int duration = count * 2;
            snes->autoJoyTimer = snes->autoJoyTimer <= duration
                                   ? 0 : snes->autoJoyTimer - duration;
        }
        pos += count * 2;
        if (pos == 1364) {
            pos = 0;
            if (++snes->vPos == 262) {
                snes->vPos = 0;
                s_frames++;
            }
        }
        snes->hPos = pos;
        ticks -= count;
    }
}
/* ROM instructions, DMA, NMI and APU remain the pinned reference engine.
 * Co-simulation checkpoints are omitted: standalone has no coordinator. */
static bool run_one_frame(void) {
    Snes *snes = g_snes;
    Interp816 *cpu = g_ref_cpu;
    uint64_t target = s_frames + 1;
    long guard = 20000000;
    while (s_frames < target && guard-- > 0) {
        int cyc = interp816_runOpcode(cpu);
        s_cpu_insns++;
        if (cyc <= 0) cyc = 1;
        int master = cyc * 8;
        g_ref_cycles += (uint64_t)cyc;
        g_ref_master_cycles += (uint64_t)master;
        sinx_fast_advance_beam(snes,master);
        snes->apuCatchupCycles += (double)master * kApuCyclesPerMaster;
        snes_catchupApu(snes);
    }
    return guard > 0;
}


#define SINX_DIR "sdmc:/switch/SiNXstrals"
#define SINX_ROM_PATH SINX_DIR "/lufia2.sfc"
#define SINX_SAVE_PATH SINX_DIR "/lufia2.srm"
/* SINX_STANDALONE_012_SWITCH_LINK_ABI_FIX:
 * This patched SDL3 build contains an optional project trace hook.
 * Define it here without importing the old ROM-derived game executable. */
static FILE *s_log;
void lufia2_nx_trace(const char *message) {
    if (s_log && message) {
        fprintf(s_log, "[SINX SDL] %s\n", message);
        fflush(s_log);
    }
}
static SDL_Gamepad *s_pad;
static SDL_AudioStream *s_audio;
static SDL_Texture *s_picture;
static SDL_Renderer *s_renderer;
static SDL_Window *s_window;
static uint8_t *s_rom_data;

static void sinx_log(const char *line) {
    if (s_log) { fprintf(s_log,"%s\n",line); fflush(s_log); }
    fprintf(stderr,"%s\n",line);
}
static void sinx_controller_open(void) {
    if (s_pad) return;
    int count=0;
    SDL_JoystickID *ids=SDL_GetGamepads(&count);
    if (ids && count>0) s_pad=SDL_OpenGamepad(ids[0]);
    if (ids) SDL_free(ids);
}
static uint16_t sinx_button_state(void) {
    if (!s_pad) return 0;
    uint16_t buttons=0;
#define BUTTON(sdl, snes) if (SDL_GetGamepadButton(s_pad,SDL_GAMEPAD_BUTTON_##sdl)) buttons |= (snes)
    /* Switch-labelled face buttons, matching the tested 0.6.6 mapping. */
    BUTTON(SOUTH, 0x100); BUTTON(EAST, 0x001);
    BUTTON(WEST,  0x200); BUTTON(NORTH,0x002);
    BUTTON(BACK,  0x004); BUTTON(START,0x008);
    BUTTON(DPAD_UP,0x010); BUTTON(DPAD_DOWN,0x020);
    BUTTON(DPAD_LEFT,0x040); BUTTON(DPAD_RIGHT,0x080);
    BUTTON(LEFT_SHOULDER,0x400); BUTTON(RIGHT_SHOULDER,0x800);
#undef BUTTON
    return buttons;
}
static void sinx_load_sram(void) {
    Cart *cart=g_snes->cart;
    if (!cart || !cart->ram || !cart->ramSize || cart->ramSize>1024*1024) return;
    FILE *f=fopen(SINX_SAVE_PATH,"rb");
    if (!f) return;
    size_t got=fread(cart->ram,1,cart->ramSize,f);
    fclose(f);
    if (got==cart->ramSize) sinx_log("[SINX] SRAM loaded");
}
static void sinx_save_sram(void) {
    Cart *cart=g_snes ? g_snes->cart : NULL;
    if (!cart || !cart->ram || !cart->ramSize || cart->ramSize>1024*1024) return;
    FILE *f=fopen(SINX_SAVE_PATH,"wb");
    if (!f) return;
    size_t wrote=fwrite(cart->ram,1,cart->ramSize,f);
    fclose(f);
    if (wrote!=cart->ramSize) sinx_log("[SINX] SRAM write incomplete");
}
static void sinx_audio_output(void) {
    Dsp *dsp=g_snes->apu->dsp;
    uint32_t available=dsp->sampleWrite-dsp->sampleRead;
    if (available>DSP_SAMPLE_RING) { /* preserve newest samples if stalled */
        dsp->sampleRead=dsp->sampleWrite-DSP_SAMPLE_RING/2;
        available=DSP_SAMPLE_RING/2;
    }
    if (!s_audio) { /* still consume to prevent ring overflow */
        int16_t scratch[1024*2];
        while (available) {
            uint32_t n=available>1024?1024:available;
            dsp_getSamples(dsp,scratch,(int)n);
            available-=n;
        }
        return;
    }
    int queued=SDL_GetAudioStreamQueued(s_audio);
    if (queued>32040*4/3) SDL_ClearAudioStream(s_audio);
    /* Keep one native sample block buffered so APU catch-up can be bursty. */
    uint32_t want=available>534 ? available-534 : 0;
    int16_t samples[1024*2];
    while (want) {
        uint32_t n=want>1024?1024:want;
        dsp_getSamples(dsp,samples,(int)n);
        sinx_v051_audio_put(s_audio,samples,(int)n*2*(int)sizeof(int16_t));
        want-=n;
    }
}
static int sinx_initialize_runtime(void) {
    uint32_t rom_size=0;
    s_rom_data=read_file(SINX_ROM_PATH,&rom_size);
    if (!s_rom_data || rom_size<32768 || rom_size>8*1024*1024 || (rom_size%32768)) {
        sinx_log("[SINX] Missing/invalid headerless ROM: " SINX_ROM_PATH);
        return -1;
    }
    g_snes=sinx_v050_snes_init(g_ram);
    if (!g_snes) return -1;
    cart_set_master_clock_source(g_snes->cart,&g_ref_master_cycles);
    g_ppu=g_snes->ppu;
    if (!snes_loadRom(g_snes,s_rom_data,(int)rom_size)) {
        sinx_log("[SINX] snes_loadRom failed"); return -1;
    }
    snes_reset(g_snes,true);
    s_render_video=true;
    /* Drawing target initialized per frame by sinx_v053_video_begin_frame(). */
    g_ref_cpu=sinx_v050_interp_init(NULL,bus_read,bus_write);
    if (!g_ref_cpu) return -1;
    interp816_set_brk_hook_enabled(g_ref_cpu,false);
    interp816_reset(g_ref_cpu);
    sinx_load_sram();
    sinx_log("[SINX] 65816 + PPU + APU initialized using user ROM");
    return 0;
}
int main(int argc,char **argv) {
    (void)argc;(void)argv;
    s_log=fopen(SINX_DIR "/standalone.log","w");
    sinx_log("[SINX] STANDALONE_SWITCH_012_BOOT");
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMEPAD)) {
        sinx_log(SDL_GetError()); return 1;
    }
    s_window=SDL_CreateWindow("SiNXstrals standalone",1280,720,SDL_WINDOW_FULLSCREEN);
    s_renderer=s_window ? SDL_CreateRenderer(s_window,NULL):NULL;
    if (!s_renderer) { sinx_log(SDL_GetError()); return 1; }
    SDL_SetRenderVSync(s_renderer,1);
    s_picture=SDL_CreateTexture(s_renderer,SDL_PIXELFORMAT_XRGB8888,
                                SDL_TEXTUREACCESS_STREAMING,SINX_V053_WIDE_WIDTH,224);
    if (!s_picture) { sinx_log(SDL_GetError()); return 1; }
    SDL_SetTextureScaleMode(s_picture,SDL_SCALEMODE_NEAREST);
    /* XRGB data is opaque even though the source high byte is zero. */
    if (!SDL_SetTextureBlendMode(s_picture,SDL_BLENDMODE_NONE)) {
        sinx_log(SDL_GetError()); return 1;
    }
    SDL_AudioSpec audio_spec={SDL_AUDIO_S16,2,32040};
    s_audio=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                       &audio_spec,NULL,NULL);
    if (s_audio) SDL_ResumeAudioStreamDevice(s_audio);
    else sinx_log("[SINX] Audio unavailable; continuing with video");
    sinx_controller_open();
    if (sinx_initialize_runtime()!=0) return 1;
    bool running=true;
    uint64_t last_save=0;
    const uint64_t frame_ns=16639267ULL; /* SNES NTSC ~60.1 Hz */
    uint64_t next_tick=SDL_GetTicksNS();
    uint64_t perf_start=next_tick, perf_emu_ns=0, perf_max_ns=0;
    uint32_t perf_frame_count=0;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type==SDL_EVENT_QUIT) running=false;
            if (ev.type==SDL_EVENT_GAMEPAD_ADDED) sinx_controller_open();
            if (ev.type==SDL_EVENT_GAMEPAD_REMOVED && s_pad) {
                SDL_CloseGamepad(s_pad); s_pad=NULL; sinx_controller_open();
            }
        }
        if (!running) break;
        if (!s_pad) sinx_controller_open();
        g_snes->input1_currentState=sinx_button_state();
        uint64_t emu_begin=SDL_GetTicksNS();
        sinx_v053_video_begin_frame();
        if (!run_one_frame()) { sinx_log("[SINX] CPU frame guard exceeded"); break; }
        uint64_t emu_elapsed=SDL_GetTicksNS()-emu_begin;
        perf_emu_ns+=emu_elapsed;
        if (emu_elapsed>perf_max_ns) perf_max_ns=emu_elapsed;
        perf_frame_count++;
        sinx_audio_output();
        if (!sinx_rescue_upload(s_picture,NULL,sinx_v053_pixels,SINX_V053_WIDE_PITCH)) {
            sinx_log("[SINX VIDEO] SDL_UpdateTexture failed");
            sinx_log(SDL_GetError()); break;
        }
        SDL_SetRenderDrawColor(s_renderer,0,0,0,255);
        SDL_RenderClear(s_renderer);
        SDL_FRect dst={160.f,0.f,960.f,720.f}; /* true 4:3; no art stretch to 16:9 */
        if (!sinx_rescue_draw(s_renderer,s_picture,NULL,&dst)) {
            sinx_log("[SINX VIDEO] SDL_RenderTexture failed");
            sinx_log(SDL_GetError()); break;
        }
        if (!sinx_v050_present(s_renderer)) {
            sinx_log("[SINX VIDEO] SDL_RenderPresent failed");
            sinx_log(SDL_GetError()); break;
        }
        if (perf_frame_count>=60) {
            uint64_t end=SDL_GetTicksNS();
            uint32_t active_pixels=0;
            for (size_t i=0;i<(size_t)SINX_V053_WIDE_WIDTH*224;i++) {
                if (((const uint32_t*)sinx_v053_pixels)[i]&0x00ffffffu) active_pixels++;
            }
            double secs=(double)(end-perf_start)/1000000000.0;
            double fps=secs>0.0 ? (double)perf_frame_count/secs : 0.0;
            double emu_ms=(double)perf_emu_ns/(double)perf_frame_count/1000000.0;
            double max_ms=(double)perf_max_ns/1000000.0;
            char msg[320];
            snprintf(msg,sizeof(msg),
                     "[SINX PERF] frame=%llu fps=%.1f cpu_apu_ms=%.2f max_ms=%.2f visible_rgb=%u audio_queued=%d",
                     (unsigned long long)s_frames,fps,emu_ms,max_ms,active_pixels,
                     s_audio ? SDL_GetAudioStreamQueued(s_audio) : -1);
            sinx_log(msg);
            perf_start=end; perf_emu_ns=0; perf_max_ns=0; perf_frame_count=0;
        }
        if (s_frames-last_save>=600) { sinx_save_sram(); last_save=s_frames; }
        next_tick+=frame_ns;
        uint64_t now=SDL_GetTicksNS();
        if (next_tick>now && next_tick-now<frame_ns) SDL_DelayNS(next_tick-now);
        else if (now>next_tick+frame_ns) next_tick=now;
    }
    sinx_save_sram();
    if (s_log) { fprintf(s_log,"[SINX] exited after %llu frames\n",
                         (unsigned long long)s_frames); fclose(s_log); }
    if (s_audio) SDL_DestroyAudioStream(s_audio);
    if (s_pad) SDL_CloseGamepad(s_pad);
    if (s_picture) SDL_DestroyTexture(s_picture);
    if (s_renderer) SDL_DestroyRenderer(s_renderer);
    if (s_window) SDL_DestroyWindow(s_window);
    SDL_Quit();
    return 0;
}
