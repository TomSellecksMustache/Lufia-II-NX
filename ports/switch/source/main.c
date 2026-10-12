/* Original ROM-free Nintendo Switch diagnostic host for Lufia-II-NX.
 * This does NOT run Lufia II until a real nxgame_* runtime bridge exists.
 * SDL2 via devkitPro supplies video/audio. libnx supplies controller input. */
#include <switch.h>
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "game_bridge.h"

#define W 1280
#define H 720
#define ROM_PATH "sdmc:/switch/Lufia-II-NX/lufia2.sfc"
#define SAVE_DIR "sdmc:/switch/Lufia-II-NX/saves"

enum { PAD_B=1, PAD_Y=2, PAD_SELECT=4, PAD_START=8,
       PAD_UP=16, PAD_DOWN=32, PAD_LEFT=64, PAD_RIGHT=128,
       PAD_A=256, PAD_X=512, PAD_L=1024, PAD_R=2048 };

/* A tiny original pixel font, enough to display the hardware diagnostic. */
typedef struct {char c; uint8_t row[7];} Glyph;
static const Glyph glyphs[] = {
{'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},
{'C',{15,16,16,16,16,16,15}},{'D',{30,17,17,17,17,17,30}},
{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
{'G',{15,16,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},
{'I',{31,4,4,4,4,4,31}},{'J',{7,2,2,2,18,18,12}},
{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
{'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},
{'O',{14,17,17,17,17,17,14}},{'P',{30,17,17,30,16,16,16}},
{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
{'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},
{'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},
{'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
{'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
{'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},
{'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},
{'4',{2,6,10,18,31,2,2}},{'5',{31,16,30,1,1,17,14}},
{'6',{6,8,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},
{'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,2,12}},
{':',{0,4,4,0,4,4,0}},{'.',{0,0,0,0,0,12,12}},
{'/',{1,1,2,4,8,16,16}},{'-',{0,0,0,31,0,0,0}},
{' ',{0,0,0,0,0,0,0}}
};
static void text(SDL_Renderer *r, int x, int y, int s, const char *str) {
    for (; *str; str++,x+=s*6) {
        char c=*str;
        if (c>='a' && c<='z') c=(char)(c-32);
        const Glyph *g=NULL;
        for (size_t i=0;i<sizeof glyphs/sizeof *glyphs;i++) if(glyphs[i].c==c){g=&glyphs[i];break;}
        if(!g)continue;
        for(int iy=0;iy<7;iy++)for(int ix=0;ix<5;ix++)if(g->row[iy]&(1<<(4-ix))) {
            SDL_Rect p={x+ix*s,y+iy*s,s,s}; SDL_RenderFillRect(r,&p);
        }
    }
}
static volatile int beep=0;
static SDL_atomic_t game_audio_active;
static void audio_callback(void *userdata, uint8_t *stream, int len) {
    (void)userdata;
    int16_t *out=(int16_t*)stream;
    int frames=len/(2*(int)sizeof(int16_t));
    if (SDL_AtomicGet(&game_audio_active)) {
        memset(out,0,(size_t)len);
        /* A real core MUST implement thread-safe sound generation. */
        (void)nxgame_audio(out,(size_t)frames,48000);
        return;
    }
    static int phase=0;
    for(int i=0;i<frames;i++){
        int16_t v=beep?((phase++/55)%2?600:-600):0;
        out[i*2]=v;out[i*2+1]=v;
    }
}
static uint16_t map_keys(uint64_t b) {
    uint16_t s=0;
    if(b&HidNpadButton_A)s|=PAD_A;
    if(b&HidNpadButton_B)s|=PAD_B;
    if(b&HidNpadButton_X)s|=PAD_X;
    if(b&HidNpadButton_Y)s|=PAD_Y;
    if(b&HidNpadButton_Plus)s|=PAD_START;
    if(b&HidNpadButton_Minus)s|=PAD_SELECT;
    if(b&HidNpadButton_L)s|=PAD_L;
    if(b&HidNpadButton_R)s|=PAD_R;
    if(b&HidNpadButton_Up)s|=PAD_UP;
    if(b&HidNpadButton_Down)s|=PAD_DOWN;
    if(b&HidNpadButton_Left)s|=PAD_LEFT;
    if(b&HidNpadButton_Right)s|=PAD_RIGHT;
    return s;
}
int main(void){
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER)!=0)return 1;
    SDL_Window *window=SDL_CreateWindow("Lufia II NX",0,0,W,H,SDL_WINDOW_FULLSCREEN);
    if(!window){SDL_Quit();return 2;}
    SDL_Renderer *ren=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!ren)ren=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!ren){SDL_DestroyWindow(window);SDL_Quit();return 3;}
    SDL_AudioSpec spec={0};spec.freq=48000;spec.format=AUDIO_S16SYS;spec.channels=2;spec.samples=1024;spec.callback=audio_callback;
    SDL_AudioDeviceID audio=SDL_OpenAudioDevice(NULL,0,&spec,NULL,0);
    if(audio)SDL_PauseAudioDevice(audio,0);
    bool rom_found=false; FILE *f=fopen(ROM_PATH,"rb");if(f){rom_found=true;fclose(f);}
    bool game=rom_found&&nxgame_init(ROM_PATH);
    SDL_AtomicSet(&game_audio_active,game?1:0);
    SDL_Texture *frame=NULL;int tw=0,th=0;
    bool alive=true;
    while(appletMainLoop()&&alive){
        SDL_Event ev;while(SDL_PollEvent(&ev))if(ev.type==SDL_QUIT)alive=false;
        padUpdate(&pad);uint64_t b=padGetButtons(&pad);uint16_t keys=map_keys(b);
        if((b&HidNpadButton_Plus)&&(b&HidNpadButton_Minus))alive=false;
        beep=((b&HidNpadButton_B)!=0)&&!game;
        SDL_SetRenderDrawColor(ren,12,19,35,255);SDL_RenderClear(ren);
        if(game){
            nxgame_input(keys);nxgame_step();
            int w=0,h=0;const uint32_t *pixels=nxgame_frame(&w,&h);
            if(pixels&&w>0&&h>0&&w<=1024&&h<=1024){
                if(w!=tw||h!=th){if(frame)SDL_DestroyTexture(frame);frame=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);tw=w;th=h;}
                if(frame){SDL_UpdateTexture(frame,NULL,pixels,w*4);SDL_Rect dst={0,0,W,H};int scaled_w=(H*w)/h;dst.w=scaled_w;dst.x=(W-scaled_w)/2;SDL_RenderCopy(ren,frame,NULL,&dst);}
            }
        }else{
            SDL_SetRenderDrawColor(ren,213,229,247,255);
            text(ren,78,70,5,"LUFIA II NX");text(ren,78,146,3,"NATIVE SWITCH DIAGNOSTIC");
            text(ren,78,236,3,rom_found?"ROM DETECTED ON SD":"ROM NOT FOUND ON SD");
            text(ren,78,290,3,"GAME CORE NOT INTEGRATED");
            text(ren,78,355,3,"DPAD AND FACE BUTTONS TEST");
            int bx=78,by=430;
            uint16_t masks[]={PAD_UP,PAD_DOWN,PAD_LEFT,PAD_RIGHT,PAD_A,PAD_B,PAD_X,PAD_Y,PAD_L,PAD_R};
            const char *names[]={"UP","DOWN","LEFT","RIGHT","A","B","X","Y","L","R"};
            for(int i=0;i<10;i++){
                int x=bx+(i%5)*218,y=by+(i/5)*66;
                bool down=keys&masks[i];
                SDL_SetRenderDrawColor(ren,down?60:48,down?191:66,down?121:93,255);
                SDL_Rect rect={x,y,195,54}; SDL_RenderFillRect(ren,&rect);
                SDL_SetRenderDrawColor(ren,255,255,255,255);text(ren,x+14,y+15,3,names[i]);
            }
            SDL_SetRenderDrawColor(ren,213,229,247,255);
            text(ren,78,613,3,"B TESTS SOUND");text(ren,78,654,2,"PLUS AND MINUS TO EXIT");
        }
        SDL_RenderPresent(ren);
    }
    if(audio){
        SDL_LockAudioDevice(audio);
        SDL_AtomicSet(&game_audio_active,0);
        SDL_UnlockAudioDevice(audio);
        SDL_CloseAudioDevice(audio);
    }
    if(game){nxgame_save();nxgame_close();}
    if(frame)SDL_DestroyTexture(frame);
    SDL_DestroyRenderer(ren);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
