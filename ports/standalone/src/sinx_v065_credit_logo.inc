/* SINX_V065_TITLE_BLACK_WYZECORP: screen-space native splash overlay.
 * The image is compiled into the NRO; the SD ZIP needs no separate PNG.
 */
#include <SDL3/SDL.h>
static bool sinx_v065_show_credit_logo;
static SDL_Texture *sinx_v065_credit_tex;
/* Sprite data declarations are generated below. */
static bool sinx_v065_is_credit_card(const uint8_t *pixels) {
    /* 342x224 source. Fixed original game legal card has white title/legal
       text in the upper middle and blank black space below. Conservative
       visual check prevents overlay from appearing during normal gameplay. */
    unsigned title=0, top=0,bottom=0;
    for(int y=0;y<224;y+=2) for(int x=0;x<342;x+=2) {
        const uint8_t *px=pixels+((size_t)y*342u+(unsigned)x)*4u;
        bool bright=px[0]>185 && px[1]>185 && px[2]>185;
        if(!bright)continue;
        if(y>=76 && y<144 && x>=74 && x<=268) title++;
        if(y<65) top++;
        if(y>=152) bottom++;
    }
    return title>90 && top<3 && bottom<3;
}
static void sinx_v065_credit_detect(uint8_t *pixels) {
    bool card=sinx_v065_is_credit_card(pixels);
    sinx_v065_show_credit_logo=card;
    if(card) {
        /* Make the legal-card background genuinely RGB 0,0,0. Leave bright
           text pixels intact and never mask another scene. */
        for(size_t i=0;i<(size_t)342*224;i++) {
            uint8_t *p=pixels+i*4;
            if(p[0]<30 && p[1]<30 && p[2]<30) p[0]=p[1]=p[2]=0;
        }
    }
}
static void sinx_v065_draw_credit(SDL_Renderer *renderer) {
    if(!sinx_v065_show_credit_logo || !renderer)return;
    if(!sinx_v065_credit_tex) {
        uint8_t raw[128*128*4]={0};
        for(unsigned i=0;i<SINX_V065_PIXEL_COUNT;i++) {
            unsigned x=sinx_v065_pixel_data[i][0];
            unsigned y=sinx_v065_pixel_data[i][1];
            unsigned c=sinx_v065_pixel_data[i][2];
            if(x>=128 || y>=128 || c>=SINX_V065_PALETTE_COUNT)continue;
            uint8_t *dest=raw+(y*128+x)*4;
            dest[0]=sinx_v065_palette[c][0];
            dest[1]=sinx_v065_palette[c][1];
            dest[2]=sinx_v065_palette[c][2];
            dest[3]=255;
        }
        sinx_v065_credit_tex=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
                                               SDL_TEXTUREACCESS_STATIC,128,128);
        if(!sinx_v065_credit_tex)return;
        SDL_UpdateTexture(sinx_v065_credit_tex,NULL,raw,128*4);
        SDL_SetTextureBlendMode(sinx_v065_credit_tex,SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(sinx_v065_credit_tex,SDL_SCALEMODE_NEAREST);
    }
    int w=1280,h=720;
    SDL_GetRenderOutputSize(renderer,&w,&h);
    /* Match the supplied 1280x720 mockup: source 128x128 -> 256 square;
       the art's transparent surround places visible logo below the text. */
    SDL_FRect dst={(float)(w-256)*.5f,(float)h*474.f/720.f,256.f,256.f};
    SDL_RenderTexture(renderer,sinx_v065_credit_tex,NULL,&dst);
}
