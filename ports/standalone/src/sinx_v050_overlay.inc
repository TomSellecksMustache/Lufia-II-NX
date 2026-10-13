/* SINX_V052_169_FULLSCREEN: v0.5.2 retains v0.5.1 save/audio/menu. */
/* SINX_V051_GUARDED_REPAIR v0.5.1 menu, save, presentation and debug */
/* SINX_V050_CART_ROM_SIZE_SAVE_PATH_FIX_V1: preserve SRAM, use Cart.romSize. */
/* SINX_V050_MENU_GPU_SAVE_V1 -- Switch-only standalone overlay.
 * Original frontend helper still handles upload. Renderer handles final scaling.
 * This is a source-first integration; on-device save/load testing is required.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#define SINX_STATE_PATH "sdmc:/switch/SiNXstrals/saves/quick.sxs"
/* v0.5.1 sound diagnostics: atomic counters only on audio callback thread. */
static _Atomic unsigned long sinx_v051_audio_puts;
static _Atomic unsigned long sinx_v051_audio_bytes;
static _Atomic unsigned long sinx_v051_audio_low_events;
static _Atomic unsigned long sinx_v051_audio_errors;
static _Atomic int sinx_v051_audio_last_queued;
static _Atomic int sinx_v051_audio_src_hz;
static _Atomic int sinx_v051_audio_dst_hz;
static bool sinx_v051_audio_put(SDL_AudioStream *st,const void *data,int len) {
    const unsigned long nth=atomic_fetch_add(&sinx_v051_audio_puts,1)+1;
    if (nth==1 || (nth % 64u)==0) {
        const int queued=SDL_GetAudioStreamQueued(st);
        atomic_store(&sinx_v051_audio_last_queued,queued);
        if (queued>=0 && queued<2048)
            atomic_fetch_add(&sinx_v051_audio_low_events,1);
    }
    if (len>0) atomic_fetch_add(&sinx_v051_audio_bytes,(unsigned long)len);
    /* SDL stream formats sampled once; no disk work in audio thread. */
    if (atomic_load(&sinx_v051_audio_src_hz)==0) {
        SDL_AudioSpec in={0},out={0};
        if (SDL_GetAudioStreamFormat(st,&in,&out)) {
            atomic_store(&sinx_v051_audio_src_hz,in.freq);
            atomic_store(&sinx_v051_audio_dst_hz,out.freq);
        }
    }
    bool ok=SDL_PutAudioStreamData(st,data,len);
    if (!ok) atomic_fetch_add(&sinx_v051_audio_errors,1);
    return ok;
}

#define SINX_STATE_TEMP "sdmc:/switch/SiNXstrals/saves/quick.tmp"
#define SINX_SAVE_MAX (8u*1024u*1024u)
static Snes *sinx_machine;
static Interp816 *sinx_cpu;
static SDL_Texture *sinx_frame_texture;
static SDL_FRect sinx_frame_src, sinx_frame_dst;
static bool sinx_frame_src_valid, sinx_frame_dst_valid;
static SDL_Gamepad *sinx_pad;
static bool sinx_menu_open, sinx_menu_options;
static int sinx_menu_selection;
static bool sinx_prev_up, sinx_prev_down, sinx_prev_a, sinx_prev_b;
static bool sinx_prev_left, sinx_prev_right, sinx_prev_shader_combo;
static int sinx_confirm_action; /* 0: none, 1: save, 2: load */
static bool sinx_confirm_yes;
static bool sinx_widescreen_fill=true;
static bool sinx_minus_latched;
static uint64_t sinx_minus_started;
static bool sinx_pixel_scale = false, sinx_shader = false;
static char sinx_menu_status[40] = "";
static uint64_t sinx_menu_status_until;
static SDL_Window *sinx_output_window;

/* Both pointers are captured when the actual standalone backend initializes.
   Do NOT save only the framework's dormant Cpu shadow: interp816 is live. */
static Snes *sinx_v050_snes_init(uint8_t *ram) {
    Snes *s = snes_init(ram); sinx_machine = s; return s;
}
static Interp816 *sinx_v050_interp_init(void *mem,
          Interp816ReadHandler read, Interp816WriteHandler write) {
    Interp816 *cpu = interp816_init(mem, read, write);
    sinx_cpu = cpu; return cpu;
}

/* SaveLoadInfo is a callback interface, not a pointer-safe memcpy of Snes. */
typedef struct SinxStream {
    SaveLoadInfo sli;
    uint8_t *data;
    size_t size, pos;
    bool loading, failed;
} SinxStream;
static void sinx_transfer(SaveLoadInfo *base, void *p, size_t n) {
    SinxStream *s = (SinxStream *)base;
    if (n > s->size || s->pos > s->size - n) { s->failed=true; return; }
    if (s->loading) memcpy(p,s->data+s->pos,n);
    else memcpy(s->data+s->pos,p,n);
    s->pos+=n;
}
static uint32_t sinx_crc(const uint8_t *buf,size_t n);
static uint32_t sinx_rom_crc(void) {
    if (!sinx_machine || !sinx_machine->cart || !sinx_machine->cart->rom) return 0;
    size_t bytes=sinx_machine->cart->romSize;
    if (!bytes || bytes > 16u*1024u*1024u) return 0;
    return sinx_crc(sinx_machine->cart->rom,bytes);
}
static uint32_t sinx_crc(const uint8_t *buf, size_t n) {
    uint32_t crc=0xffffffffu;
    for (size_t i=0;i<n;i++) {
        crc ^= buf[i];
        for (int j=0;j<8;j++) crc = (crc>>1) ^ (0xedb88320u & (0u-(crc&1u)));
    }
    return ~crc;
}
/* SINX_V051_SAVE_TRANSACTION: replace-existing works on FAT/libnx too. */
#define SINX_STATE_BACKUP "sdmc:/switch/SiNXstrals/saves/quick.bak"
static void sinx_v051_debug(const char *fmt, ...) {
    static bool dir_checked=false;
    if (!dir_checked) {
        (void)mkdir("sdmc:/switch/SiNXstrals",0777);
        dir_checked=true;
    }
    FILE *f=fopen("sdmc:/switch/SiNXstrals/v051-debug.log","ab");
    if (!f) return;
    va_list ap; va_start(ap,fmt);
    vfprintf(f,fmt,ap); va_end(ap);
    fputc('\n',f); fclose(f);
}
static bool sinx_state_save(void) {
    if (!sinx_machine || !sinx_cpu || !sinx_rom_crc()) return false;
    SinxStream s={0}; s.sli.func=sinx_transfer;
    s.size=SINX_SAVE_MAX; s.data=(uint8_t*)malloc(s.size);
    if (!s.data) return false;
    snes_saveload(sinx_machine,&s.sli);
    interp816_saveload(sinx_cpu,&s.sli);
    bool ok=!s.failed && s.pos>100;
    const uint32_t content_crc=ok?sinx_crc(s.data,s.pos):0;
    const uint32_t rom_crc=ok?sinx_rom_crc():0;
    if (ok) {
        (void)mkdir("sdmc:/switch/SiNXstrals",0777);
        (void)mkdir("sdmc:/switch/SiNXstrals/saves",0777);
        FILE *f=fopen(SINX_STATE_TEMP,"wb");
        uint32_t head[5]={0x30585353u,2u,(uint32_t)s.pos,content_crc,rom_crc};
        ok=f && fwrite(head,sizeof(head),1,f)==1 &&
            fwrite(s.data,1,s.pos,f)==s.pos && fflush(f)==0;
        if (f && fclose(f)!=0) ok=false;
        if (ok) {
            struct stat old;
            bool had_old=(stat(SINX_STATE_PATH,&old)==0);
            bool moved_old=false;
            if (had_old) {
                /* Remove stale backup only after new temporary file is complete. */
                (void)remove(SINX_STATE_BACKUP);
                if (rename(SINX_STATE_PATH,SINX_STATE_BACKUP)==0)
                    moved_old=true;
                else ok=false;
            }
            if (ok && rename(SINX_STATE_TEMP,SINX_STATE_PATH)!=0) ok=false;
            if (ok) {
                FILE *check=fopen(SINX_STATE_PATH,"rb");
                uint32_t h[5]={0};
                ok=check && fread(h,sizeof(h),1,check)==1 &&
                   h[0]==head[0] && h[1]==head[1] &&
                   h[2]==head[2] && h[3]==head[3] && h[4]==head[4];
                if (check) fclose(check);
            }
            if (ok) {
                if (moved_old) (void)remove(SINX_STATE_BACKUP);
            } else if (moved_old) {
                (void)remove(SINX_STATE_PATH);
                if (rename(SINX_STATE_BACKUP,SINX_STATE_PATH)!=0)
                    sinx_v051_debug("STATE ROLLBACK FAILED errno=%d",errno);
            }
        }
        if (!ok) (void)remove(SINX_STATE_TEMP);
    }
    sinx_v051_debug("SAVE ok=%d len=%lu crc=%08lx errno=%d ticks=%llu",
         (int)ok,(unsigned long)s.pos,(unsigned long)content_crc,errno,
         (unsigned long long)SDL_GetTicks());
    free(s.data);
    return ok;
}
static bool sinx_state_load(void) {
    if (!sinx_machine || !sinx_cpu) return false;
    FILE *f=fopen(SINX_STATE_PATH,"rb");
    if (!f) return false;
    uint32_t h[5]={0};
    bool ok=fread(h,sizeof(h),1,f)==1 && h[0]==0x30585353u &&
            h[1]==2u && h[2]>100u && h[2]<=SINX_SAVE_MAX &&
            h[4]!=0 && h[4]==sinx_rom_crc();
    uint8_t *mem=ok?(uint8_t*)malloc(h[2]):NULL;
    if (!mem) ok=false;
    if (ok) {
        ok=fread(mem,1,h[2],f)==h[2] && fgetc(f)==EOF &&
           sinx_crc(mem,h[2])==h[3];
    }
    fclose(f);
    if (ok) {
        /* Check current runtime's expected byte size before mutating the guest.
           Snapshot length must match exactly as well as ROM checksum. */
        SinxStream count={0};count.sli.func=sinx_transfer;
        count.data=(uint8_t*)malloc(SINX_SAVE_MAX);
        count.size=SINX_SAVE_MAX;
        if (!count.data) ok=false;
        else {
            snes_saveload(sinx_machine,&count.sli);
            interp816_saveload(sinx_cpu,&count.sli);
            ok=!count.failed && count.pos==h[2];
            free(count.data);
        }
    }
    if (ok) {
        /* The complete machine and ACTIVE 65816 interpreter are restored. */
        SinxStream s={0};s.sli.func=sinx_transfer;s.data=mem;
        s.size=h[2];s.loading=true;
        snes_saveload(sinx_machine,&s.sli);
        interp816_saveload(sinx_cpu,&s.sli);
        ok=!s.failed && s.pos==s.size;
    }
    free(mem);
    return ok;
}

/* Five-pixel bitmap type, no font, ROM artwork or SDL_ttf dependency. */
typedef struct { char c; uint8_t bits[7]; } SinxGlyph;
static const SinxGlyph sinx_font[] = {
  {' ',{0,0,0,0,0,0,0}},
  {'A',{14,17,17,31,17,17,17}},
  {'B',{30,17,17,30,17,17,30}},
  {'C',{15,16,16,16,16,16,15}},
  {'D',{30,17,17,17,17,17,30}},
  {'E',{31,16,16,30,16,16,31}},
  {'F',{31,16,16,30,16,16,16}},
  {'G',{15,16,16,23,17,17,15}},
  {'H',{17,17,17,31,17,17,17}},
  {'I',{31,4,4,4,4,4,31}},
  {'J',{7,2,2,2,18,18,12}},
  {'K',{17,18,20,24,20,18,17}},
  {'L',{16,16,16,16,16,16,31}},
  {'M',{17,27,21,21,17,17,17}},
  {'N',{17,25,21,19,17,17,17}},
  {'O',{14,17,17,17,17,17,14}},
  {'P',{30,17,17,30,16,16,16}},
  {'Q',{14,17,17,17,21,18,13}},
  {'R',{30,17,17,30,20,18,17}},
  {'S',{15,16,16,14,1,1,30}},
  {'T',{31,4,4,4,4,4,4}},
  {'U',{17,17,17,17,17,17,14}},
  {'V',{17,17,17,17,17,10,4}},
  {'W',{17,17,17,21,21,21,10}},
  {'X',{17,17,10,4,10,17,17}},
  {'Y',{17,17,10,4,4,4,4}},
  {'Z',{31,1,2,4,8,16,31}},
  {'0',{14,17,19,21,25,17,14}},
  {'1',{4,12,4,4,4,4,14}},
  {'2',{14,17,1,2,4,8,31}},
  {'3',{30,1,1,6,1,1,30}},
  {'4',{2,6,10,18,31,2,2}},
  {'5',{31,16,30,1,1,17,14}},
  {'6',{14,16,30,17,17,17,14}},
  {'7',{31,1,2,4,8,8,8}},
  {'8',{14,17,17,14,17,17,14}},
  {'9',{14,17,17,15,1,1,14}},
  {'>',{16,8,4,2,4,8,16}},
  {'.',{0,0,0,0,0,12,12}},
  {':',{0,4,4,0,4,4,0}},
};
static void sinx_rect(SDL_Renderer *r,float x,float y,float w,float h,
                      uint8_t red,uint8_t green,uint8_t blue) {
    SDL_FRect f={x,y,w,h};SDL_SetRenderDrawColor(r,red,green,blue,255);
    SDL_RenderFillRect(r,&f);
}
static void sinx_text(SDL_Renderer *r,const char *s,int x,int y,int scale,
                      uint8_t cr,uint8_t cg,uint8_t cb) {
    for (;*s;s++,x+=6*scale) {
        char c=*s;
        if (c>='a'&&c<='z') c=(char)(c-'a'+'A');
        const SinxGlyph *g=NULL;
        for (size_t k=0;k<sizeof(sinx_font)/sizeof(sinx_font[0]);k++)
            if (sinx_font[k].c==c) {g=&sinx_font[k];break;}
        if (!g) continue;
        for (int iy=0;iy<7;iy++) for (int ix=0;ix<5;ix++)
            if (g->bits[iy]&(1u<<(4-ix)))
                sinx_rect(r,(float)(x+ix*scale),(float)(y+iy*scale),
                          (float)scale,(float)scale,cr,cg,cb);
    }
}
static void sinx_notice(const char *message) {
    SDL_strlcpy(sinx_menu_status,message,sizeof(sinx_menu_status));
    sinx_menu_status_until=SDL_GetTicks()+1800u;
}
static void sinx_options_apply(void) {
    SDL_SetHint("L2NX_HD2D",sinx_shader?"1":"0");
}
static void sinx_menu_paint(SDL_Renderer *r) {
    int rw=1280,rh=720;
    SDL_GetRenderOutputSize(r,&rw,&rh);
    if (rw<1||rh<1) return;
    /* Logical coordinate system ensures the menu scales with future dock mode. */
    float sx=(float)rw/1280.f,sy=(float)rh/720.f;
    SDL_SetRenderScale(r,sx,sy);
    sinx_rect(r,288,130,704,456,5,11,40);
    sinx_rect(r,295,137,690,442,185,146,65);
    sinx_rect(r,301,143,678,430,10,27,90);
    sinx_rect(r,308,150,664,416,23,43,116);
    sinx_rect(r,315,158,650,400,10,26,76);
    sinx_text(r,"SINXSTRALS",447,190,4,244,214,136);
    sinx_text(r,sinx_menu_options?"OPTIONS":"GAME MENU",438,264,3,252,246,206);
    const char *main_opts[3]={"SAVE STATE","LOAD STATE","OPTIONS"};
    const char *sub_opts[4]={"PIXEL SMOOTH","SHADERS","NATIVE 16:9","BACK"};
    int n=sinx_menu_options?4:3;
    for (int i=0;i<n;i++) {
        int y=337+i*43;
        if (i==sinx_menu_selection) {
            sinx_rect(r,355,(float)y-7,570,35,43,66,134);
            sinx_rect(r,361,(float)y-4,558,29,18,42,98);
            sinx_text(r,">",379,y,2,248,205,104);
        }
        const char *label=sinx_menu_options?sub_opts[i]:main_opts[i];
        sinx_text(r,label,423,y,2,239,232,206);
        if (sinx_menu_options && i==0)
            sinx_text(r,sinx_pixel_scale?"ON":"OFF",767,y,2,245,208,124);
        if (sinx_menu_options && i==1)
            sinx_text(r,sinx_shader?"ON":"OFF",767,y,2,245,208,124);
        if (sinx_menu_options && i==2)
            sinx_text(r,sinx_widescreen_fill?"ON":"OFF",767,y,2,245,208,124);
    }
    /* SINX_V051_CONFIRM: modal choice, default NO prevents accidental overwrite. */
    if (sinx_confirm_action) {
        sinx_rect(r,367,303,545,207,178,140,67);
        sinx_rect(r,374,310,531,193,10,24,76);
        sinx_text(r,sinx_confirm_action==1?"OVERWRITE STATE?":"LOAD LAST STATE?",
                  404,341,2,246,222,153);
        sinx_rect(r,sinx_confirm_yes?438:669,414,177,40,47,71,137);
        sinx_text(r,"YES",474,426,2,252,228,180);
        sinx_text(r,"NO",715,426,2,252,228,180);
    }
    if (SDL_GetTicks()<sinx_menu_status_until)
        sinx_text(r,sinx_menu_status,378,523,2,254,216,135);
    SDL_SetRenderScale(r,1.0f,1.0f);
}
static SDL_Gamepad *sinx_current_pad(void) {
    if (sinx_pad && SDL_GamepadConnected(sinx_pad)) return sinx_pad;
    sinx_pad=NULL;
    int n=0;
    SDL_JoystickID *ids=SDL_GetGamepads(&n);
    if (ids && n>0) sinx_pad=SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
    return sinx_pad;
}
static bool sinx_button(SDL_Gamepad *p,SDL_GamepadButton b) {
    return p && SDL_GetGamepadButton(p,b);
}
static void sinx_menu_input(void) {
    SDL_Gamepad *p=sinx_current_pad();
    const bool minus=sinx_button(p,SDL_GAMEPAD_BUTTON_BACK);
    const uint64_t now=SDL_GetTicks();
    if (!minus) {sinx_minus_started=0;sinx_minus_latched=false;}
    else if (!sinx_minus_started) sinx_minus_started=now;
    else if (!sinx_menu_open && !sinx_minus_latched &&
             now-sinx_minus_started>=2000u) {
        sinx_menu_open=true;sinx_menu_options=false;
        sinx_menu_selection=0;sinx_minus_latched=true;
        sinx_confirm_action=0;
        sinx_prev_up=sinx_prev_down=sinx_prev_a=sinx_prev_b=true;
        sinx_prev_left=sinx_prev_right=true;
        sinx_notice("");
        sinx_v051_debug("MENU OPEN at=%llu",(unsigned long long)now);
    }
    /* Edge-triggered R + R3 shortcut works in gameplay AND the menu. */
    bool r=sinx_button(p,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    bool r3=sinx_button(p,SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    bool combo=r && r3;
    if (combo && !sinx_prev_shader_combo) {
        sinx_shader=!sinx_shader;sinx_options_apply();
        sinx_notice(sinx_shader?"SHADERS ON":"SHADERS OFF");
        sinx_v051_debug("SHADER toggle=%d via R+R3 at=%llu",
                         (int)sinx_shader,(unsigned long long)now);
    }
    sinx_prev_shader_combo=combo;
    if (!sinx_menu_open) return;
    bool up=sinx_button(p,SDL_GAMEPAD_BUTTON_DPAD_UP);
    bool down=sinx_button(p,SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    bool left=sinx_button(p,SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    bool right=sinx_button(p,SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    /* Physical Nintendo A is SDL SOUTH in this frontend; B is EAST.
       Hardware observation overrides desktop/Xbox label assumptions. */
    bool a=sinx_button(p,SDL_GAMEPAD_BUTTON_SOUTH);
    bool b=sinx_button(p,SDL_GAMEPAD_BUTTON_EAST);
    const bool next= (down&&!sinx_prev_down)||(right&&!sinx_prev_right);
    const bool prev= (up&&!sinx_prev_up)||(left&&!sinx_prev_left);
    if (sinx_confirm_action) {
        if (next || prev) sinx_confirm_yes=!sinx_confirm_yes;
        if (b&&!sinx_prev_b) {
            sinx_v051_debug("CONFIRM CANCEL kind=%d",sinx_confirm_action);
            sinx_confirm_action=0;
        } else if (a&&!sinx_prev_a) {
            if (sinx_confirm_yes) {
                int action=sinx_confirm_action;
                bool ok=action==1?sinx_state_save():sinx_state_load();
                sinx_notice(action==1?(ok?"STATE SAVED":"SAVE FAILED"):
                                      (ok?"STATE LOADED":"LOAD FAILED"));
                sinx_v051_debug("CONFIRM action=%d result=%d",action,(int)ok);
                if (ok && action==2) sinx_menu_open=false;
            }
            sinx_confirm_action=0;
        }
    } else {
        int n=sinx_menu_options?4:3;
        if (up&&!sinx_prev_up) sinx_menu_selection=(sinx_menu_selection+n-1)%n;
        if (down&&!sinx_prev_down) sinx_menu_selection=(sinx_menu_selection+1)%n;
        if (b&&!sinx_prev_b) {
            if (sinx_menu_options) {sinx_menu_options=false;sinx_menu_selection=0;}
            else sinx_menu_open=false;
        }
        if (a&&!sinx_prev_a && sinx_menu_open) {
            if (!sinx_menu_options) {
                if (sinx_menu_selection<=1) {
                    sinx_confirm_action=sinx_menu_selection+1;
                    sinx_confirm_yes=false;
                } else {sinx_menu_options=true;sinx_menu_selection=0;}
            } else {
                if (sinx_menu_selection==0) {
                    sinx_pixel_scale=!sinx_pixel_scale;
                    sinx_v051_debug("FILTER smooth=%d",(int)sinx_pixel_scale);
                } else if (sinx_menu_selection==1) {
                    sinx_shader=!sinx_shader;sinx_options_apply();
                    sinx_v051_debug("SHADER toggle=%d via menu",(int)sinx_shader);
                } else if (sinx_menu_selection==2) {
                    sinx_widescreen_fill=!sinx_widescreen_fill;
                    sinx_notice(sinx_widescreen_fill?"NATIVE 16:9 ON":"NATIVE 16:9 OFF");
                    sinx_v051_debug("16:9 fill=%d",(int)sinx_widescreen_fill);
                } else {sinx_menu_options=false;sinx_menu_selection=0;}
            }
        }
    }
    sinx_prev_up=up;sinx_prev_down=down;
    sinx_prev_left=left;sinx_prev_right=right;
    sinx_prev_a=a;sinx_prev_b=b;
}

/* Every frame is GPU-scaled once; no CPU-side 720p pixel expansion. Native
   resolution can be negotiated later without changing framebuffer storage. */
/* GPU scaling. OFF means native-size center; ON means 720p presentation.
 * Smooth applies LINEAR reconstruction; crisp uses integer NEAREST pixels. */
/* SINX_V052_169_FULLSCREEN: full 16:9 fill, stretched SNES image.
 * True extra game-world visibility requires future PPU/game-specific work. */
static bool sinx_v050_draw(SDL_Renderer *r,SDL_Texture *tex,
                          const SDL_FRect *src,const SDL_FRect *old_dst) {
    (void)old_dst; /* Old frontend supplies a 4:3 destination. Ignore it. */
    if (!r || !tex) return false;
    sinx_frame_texture=tex;
    sinx_frame_src_valid=(src!=NULL);
    if (src) sinx_frame_src=*src;
    bool ok=SDL_SetRenderTarget(r,NULL);
    ok=SDL_SetRenderLogicalPresentation(r,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&ok;
    ok=SDL_SetRenderViewport(r,NULL)&&ok;
    ok=SDL_SetRenderClipRect(r,NULL)&&ok;
    ok=SDL_SetRenderScale(r,1.0f,1.0f)&&ok;
    if (!ok) {
        static bool warned=false;
        if (!warned){warned=true;sinx_v051_debug("VIDEO_052 SDL state: %s",SDL_GetError());}
        return false;
    }
    int w=0,h=0;
    if (!SDL_GetRenderOutputSize(r,&w,&h)||w<1||h<1)return false;
    SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(tex,sinx_pixel_scale?SDL_SCALEMODE_LINEAR:SDL_SCALEMODE_NEAREST);
    SDL_FRect dst={0,0,(float)w,(float)h};
    if (!sinx_widescreen_fill) {
        float dw=(float)h*4.0f/3.0f,dh=(float)h;
        if (dw>(float)w){dw=(float)w;dh=(float)w*0.75f;}
        dst.x=((float)w-dw)*0.5f;dst.y=((float)h-dh)*0.5f;
        dst.w=dw;dst.h=dh;
    }
    sinx_frame_dst=dst;sinx_frame_dst_valid=true;
    SDL_SetRenderDrawColor(r,0,0,0,255);
    SDL_RenderClear(r);
    static int oldw=-1,oldh=-1,oldmode=-1,oldsmooth=-1;
    if (oldw!=w||oldh!=h||oldmode!=(int)sinx_widescreen_fill||
        oldsmooth!=(int)sinx_pixel_scale) {
        sinx_v051_debug("VIDEO_052 output=%dx%d dst=%.0f,%.0f,%.0fx%.0f fill169=%d smooth=%d",
            w,h,(double)dst.x,(double)dst.y,(double)dst.w,(double)dst.h,
            (int)sinx_widescreen_fill,(int)sinx_pixel_scale);
        oldw=w;oldh=h;oldmode=(int)sinx_widescreen_fill;
        oldsmooth=(int)sinx_pixel_scale;
    }
    SDL_FRect sinx_native_4x3={43.f,0.f,256.f,224.f};
    return SDL_RenderTexture(r,tex,src?src:(sinx_widescreen_fill?NULL:&sinx_native_4x3),&dst);
}

static bool sinx_v050_present(SDL_Renderer *r) {
    static bool initialized;
    if (!initialized) {
        initialized=true;
        sinx_output_window=SDL_GetRenderWindow(r);
        if (sinx_output_window) SDL_SetWindowSize(sinx_output_window,1280,720);
        sinx_options_apply();
    }
    /* SINX_V051_GUARDED_REPAIR: rate/queue diagnostics every ~3 seconds. */
    {
        static uint64_t last_tick=0;
        static unsigned frames=0;
        uint64_t tick=SDL_GetTicks();
        ++frames;
        if (!last_tick) {last_tick=tick;}
        else if (tick-last_tick>=3000u) {
            int ow=0,oh=0; SDL_GetRenderOutputSize(r,&ow,&oh);
            sinx_v051_debug("VIDEO elapsed_ms=%llu frames=%u fps_x100=%llu "
                 "output=%dx%d fill169=%d smooth=%d shaders=%d "
                 "audio_puts=%lu audio_bytes=%lu queued_last=%d "
                 "audio_low_events=%lu audio_failures=%lu src_hz=%d dst_hz=%d",
                 (unsigned long long)(tick-last_tick),frames,
                 (unsigned long long)frames*100000ull/(tick-last_tick),
                 ow,oh,(int)sinx_widescreen_fill,(int)sinx_pixel_scale,(int)sinx_shader,
                 (unsigned long)atomic_load(&sinx_v051_audio_puts),
                 (unsigned long)atomic_load(&sinx_v051_audio_bytes),
                 atomic_load(&sinx_v051_audio_last_queued),
                 (unsigned long)atomic_load(&sinx_v051_audio_low_events),
                 (unsigned long)atomic_load(&sinx_v051_audio_errors),
                 atomic_load(&sinx_v051_audio_src_hz),
                 atomic_load(&sinx_v051_audio_dst_hz));
            frames=0;last_tick=tick;
        }
    }
    SDL_PumpEvents();
    sinx_menu_input();
    if (!sinx_menu_open) return SDL_RenderPresent(r);
    /* Modal loop intentionally blocks the emulation loop at frame boundary. */
    while (sinx_menu_open) {
        SDL_SetRenderTarget(r,NULL);
        SDL_SetRenderViewport(r,NULL);
        SDL_SetRenderClipRect(r,NULL);
        SDL_SetRenderScale(r,1,1);
        SDL_SetRenderDrawColor(r,0,0,0,255);
        SDL_RenderClear(r);
        if (sinx_frame_texture)
            SDL_RenderTexture(r,sinx_frame_texture,
                  sinx_frame_src_valid?&sinx_frame_src:NULL,
                  sinx_frame_dst_valid?&sinx_frame_dst:NULL);
        sinx_menu_paint(r);
        SDL_RenderPresent(r);
        SDL_Delay(16);
        SDL_PumpEvents();
        sinx_menu_input();
    }
    /* Next game frame rebuilds the background after loading a new state. */
    return true;
}
