/*
 * launcher.c - XMB-style game launcher for Q2 / Allwinner A13 (Lakka, sun5i).
 *
 * Port of the MP5 X7 / ATJ2273S "mylauncher" (Cross Media Bar) to Linux:
 *   - draws directly to /dev/fb0 (320x240, 32bpp ARGB8888 / BGRA)
 *   - reads the built-in keys + USB pads from /dev/input/event* (evdev)
 *   - renders text with FreeType using the bundled fonts (ui.ttf / xmb.ttf)
 *   - scans /storage/roms/<system> and launches RetroArch per game
 *
 * Not affiliated with the SDK build; standalone binary (see Makefile).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <poll.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* ----------------------------------------------------------------- config -- */
#define SCR_W 320
#define SCR_H 240

#ifndef ROMDIR
#define ROMDIR   "/storage/roms"
#endif
#ifndef COREDIR
#define COREDIR  "/usr/lib/libretro"
#endif
#ifndef RABIN
#define RABIN    "/flash/retroarch"
#endif
#ifndef FONTDIR1
#define FONTDIR1 "/flash/launcher/fonts"
#endif
#ifndef FONTDIR2
#define FONTDIR2 "/storage/launcher/fonts"
#endif
#ifndef COVDIR1
#define COVDIR1  "/flash/launcher/covers"
#endif
#ifndef COVDIR2
#define COVDIR2  "/storage/covers"
#endif
#ifndef CFGPATH
#define CFGPATH  "/storage/launcher.cfg"
#endif

/* ------------------------------------------------------------------ model -- */
#define MAX_GAMES 512
#define MAX_SYS   24

typedef struct { char label[64]; char path[300]; } game_t;
typedef struct { char name[24]; char dir[24]; char core[64];
                 game_t g[MAX_GAMES]; int n; } sys_t;

static sys_t  s_sys[MAX_SYS];
static int    s_nsys = 0;
static int    s_syssel = 0;
static int    s_gsel = 0;
static int    s_top = 0;

/* systems we know: rom dir -> display name + libretro core */
typedef struct { const char *dir; const char *name; const char *core; } sysdef_t;
static const sysdef_t SYS[] = {
    { "nes",          "NES",         "fceumm_libretro.so"            },
    { "snes",         "SNES",        "snes9x2005_libretro.so"        },
    { "gba",          "GBA",         "mgba_libretro.so"              },
    { "gb",           "GB",          "gambatte_libretro.so"          },
    { "gbc",          "GBC",         "gambatte_libretro.so"          },
    { "genesis",      "Genesis",     "genesis_plus_gx_libretro.so"   },
    { "megadrive",    "Megadrive",   "genesis_plus_gx_libretro.so"   },
    { "mastersystem", "SMS",         "genesis_plus_gx_libretro.so"   },
    { "gamegear",     "GameGear",    "genesis_plus_gx_libretro.so"   },
    { "sega",         "Sega",        "genesis_plus_gx_libretro.so"   },
    { "psx",          "PlayStation", "pcsx_rearmed_libretro.so"      },
    { "ps1",          "PlayStation", "pcsx_rearmed_libretro.so"      },
    { "atari2600",    "Atari 2600",  "stella_libretro.so"            },
    { "atari7800",    "Atari 7800",  "prosystem_libretro.so"         },
    { "lynx",         "Lynx",        "handy_libretro.so"             },
    { "pcengine",     "PCE",         "mednafen_pce_fast_libretro.so" },
    { "pce",          "PCE",         "mednafen_pce_fast_libretro.so" },
    { "ngp",          "NeoGeo Pocket","mednafen_ngp_libretro.so"     },
    { "wonderswan",   "WonderSwan",  "mednafen_wswan_libretro.so"    },
    { "virtualboy",   "Virtual Boy", "mednafen_vb_libretro.so"       },
    { "vec",          "Vectrex",     "vecx_libretro.so"              },
    { "vectrex",      "Vectrex",     "vecx_libretro.so"              },
    { "arcade",       "Arcade",      "mame2003_plus_libretro.so"     },
    { "mame",         "MAME",        "mame2003_plus_libretro.so"     },
    { "coleco",       "ColecoVision","gearsystem_libretro.so"        },
    { "msx",          "MSX",         "fmsx_libretro.so"              },
};
#define NSYS ((int)(sizeof(SYS)/sizeof(SYS[0])))

/* extensions accepted when a dir isn't in the table above (fallback only) */
static const char *EXT[] = { ".nes",".fds",".unf",".smc",".sfc",".fig",".gba",".agb",
    ".gb",".gbc",".dmg",".md",".smd",".gen",".bin",".sms",".gg",".cue",".pbp",".img",
    ".iso",".a26",".a78",".lnx",".pce",".ngp",".ngc",".ws",".wsc",".vb",".vec",".zip",NULL };

/* ---------------------------------------------------------------- options -- */
static int  s_theme   = 0;      /* 0 blue 1 teal 2 sunset 3 mono */
static int  s_cov     = 1;      /* show covers */
static int  s_lang    = 0;      /* 0 ru, 1 en */

/* theme palette (from the original launcher) */
typedef struct { int top,bot,wl,wr,glow,bar; } theme_t;
static const theme_t TH[] = {
    { 0x0A0E1A, 0x1A1F3A, 0x1C4276, 0x4A2E7A, 0x15243E, 0x1E3E66 },
    { 0x06140F, 0x123028, 0x1B6B57, 0x2E6A7A, 0x123528, 0x1E5A4A },
    { 0x1A0A0E, 0x3A1A1F, 0x76341C, 0x7A2E4A, 0x3E1A24, 0x5A2E1E },
    { 0x101014, 0x24242C, 0x3A3A46, 0x565666, 0x24242C, 0x3A3A46 },
};
#define NTH ((int)(sizeof(TH)/sizeof(TH[0])))
static int C_BG_TOP,C_BG_BOT,C_WAVE_L,C_WAVE_R,C_GLOW,C_BAR_M,C_PANEL_T,C_PANEL_B,C_PANEL_EDGE;

#define C_CAT_SEL 0xFFFFFF
#define C_CAT_DIM 0x7A88A8
#define C_ITEM_SEL 0xFFFFFF
#define C_ITEM_DIM 0x8A96B4
#define C_HINT     0x6E7E9E
#define C_OK 0xFFFFFF
#define C_MUTE 0x8A96B4

/* ------------------------------------------------------------------- util -- */
static int _lerp(int a,int b,int t,int n){ return a+(b-a)*t/n; }
static int _mix(int c1,int c2,int t,int n){
    int r=_lerp((c1>>16)&0xFF,(c2>>16)&0xFF,t,n);
    int g=_lerp((c1>>8)&0xFF,(c2>>8)&0xFF,t,n);
    int b=_lerp(c1&0xFF,c2&0xFF,t,n);
    return (r<<16)|(g<<8)|b;
}
static int _isin(int deg){ static const int tb[25]={0,66,131,191,242,284,316,338,350,
    352,346,331,308,278,242,202,158,113,66,22,-22,-66,-113,-158,-202};
    int d=((deg%360)+360)%360,i=d/15; if(i>=24)return 0; return tb[i]; }
static bool _ends(const char *s,const char *e){ size_t l=strlen(s),k=strlen(e);
    return l>=k && strcasecmp(s+l-k,e)==0; }

/* ---------------------------------------------------------------- sys msg -- */
static void _die(const char *m){ perror(m); }

/* =========================================================== FRAMEBUFFER == */
static int   g_fbfd=-1;
static uint8_t *g_fb=NULL;
static size_t g_fbsz=0;
static int   g_fbw=SCR_W, g_fbh=SCR_H, g_fbbpp=32;
static int   g_fbstride=SCR_W*4;
static uint32_t *g_buf=NULL;             /* off-screen 320x240 canvas */

static int fb_open(void)
{
    struct fb_var_screeninfo vi;
    struct fb_fix_screeninfo fi;

#ifdef HOST_TEST
    g_fbw=SCR_W; g_fbh=SCR_H; g_fbbpp=32; g_fbstride=SCR_W*4;
    g_fbsz=(size_t)g_fbstride*SCR_H;
#else
    g_fbfd = open("/dev/fb0", O_RDWR);
    if (g_fbfd < 0) { _die("open /dev/fb0"); return -1; }
    if (ioctl(g_fbfd, FBIOGET_VSCREENINFO, &vi) < 0) { _die("fb vinfo"); return -1; }
    if (ioctl(g_fbfd, FBIOGET_FSCREENINFO, &fi) < 0) { _die("fb finfo"); return -1; }

    g_fbw      = vi.xres;
    g_fbh      = vi.yres;
    g_fbbpp    = vi.bits_per_pixel;
    g_fbstride = fi.line_length ? fi.line_length : g_fbw * (g_fbbpp/8);
    g_fbsz     = fi.smem_len ? fi.smem_len : (size_t)g_fbstride * g_fbh;

    g_fb = mmap(NULL, g_fbsz, PROT_READ|PROT_WRITE, MAP_SHARED, g_fbfd, 0);
    if (g_fb == MAP_FAILED) { g_fb=NULL; _die("mmap fb"); return -1; }
#endif
    if (!g_buf) g_buf = malloc(sizeof(uint32_t) * SCR_W * SCR_H);
    if (!g_buf) { _die("malloc canvas"); return -1; }

    fprintf(stderr, "fb: %dx%d bpp=%d stride=%d smem=%zu\n",
            g_fbw, g_fbh, g_fbbpp, g_fbstride, g_fbsz);
    return 0;
}

static void fb_close(void)
{
    if (g_fb && g_fb!=MAP_FAILED) { munmap(g_fb,g_fbsz); g_fb=NULL; }
    if (g_fbfd>=0) { close(g_fbfd); g_fbfd=-1; }
}

/* push the canvas to the visible framebuffer (buffer 0) */
static void fb_flip(void)
{
#ifdef HOST_TEST
    FILE *f=fopen("/tmp/launcher.ppm","wb");
    if(f){ fprintf(f,"P6\n%d %d\n255\n",SCR_W,SCR_H);
        for(int y=0;y<SCR_H;y++)for(int x=0;x<SCR_W;x++){
            uint32_t c=g_buf[(size_t)y*SCR_W+x];
            fputc((c>>16)&0xFF,f); fputc((c>>8)&0xFF,f); fputc(c&0xFF,f); }
        fclose(f); }
    return;
#else
    if (!g_fb) return;
    for (int y=0;y<SCR_H;y++)
    {
        uint32_t *dst=(uint32_t*)(g_fb + (size_t)y*g_fbstride);
        uint32_t *src=g_buf + y*SCR_W;
        if (g_fbbpp==32)
            memcpy(dst, src, SCR_W*4);
        else if (g_fbbpp==16)
        {
            uint16_t *d=(uint16_t*)dst;
            for (int x=0;x<SCR_W;x++)
            {
                uint32_t c=src[x];
                d[x]=(uint16_t)(((c>>19)&0x1F)<<11 | ((c>>10)&0x3F)<<5 | ((c>>3)&0x1F));
            }
        }
    }
#endif
}

/* =============================================================== CANVAS == */
static inline void px(int x,int y,uint32_t c)
{
    if ((unsigned)x<(unsigned)SCR_W && (unsigned)y<(unsigned)SCR_H)
        g_buf[(size_t)y*SCR_W+x]=c;
}
static void hline(int x0,int x1,int y,uint32_t c)
{
    if ((unsigned)y>=(unsigned)SCR_H) return;
    if (x0<0)x0=0; if (x1>=SCR_W)x1=SCR_W-1; if (x1<x0)return;
    uint32_t *r=g_buf+(size_t)y*SCR_W;
    for(int x=x0;x<=x1;x++) r[x]=c;
}
static void vline(int y0,int y1,int x,uint32_t c)
{
    if ((unsigned)x>=(unsigned)SCR_W) return;
    if (y0<0)y0=0; if (y1>=SCR_H)y1=SCR_H-1;
    for(int y=y0;y<=y1;y++) g_buf[(size_t)y*SCR_W+x]=c;
}
static void aa_vline(int y0,int y1,int x,int cov,uint32_t c)
{
    if (cov<=0||cov>8) return;
    if ((unsigned)x>=(unsigned)SCR_W) return;
    if (y0<0)y0=0; if (y1>=SCR_H)y1=SCR_H-1;
    uint32_t d=0xFFFFFF*(cov)/8;
    for(int y=y0;y<=y1;y++){ uint32_t *p=&g_buf[(size_t)y*SCR_W+x];
        *p=_mix(*p,c,(d>>16)&0xFF,0xFF); }
}
static void fill_rect(int x0,int y0,int x1,int y1,uint32_t c)
{ for(int y=y0;y<=y1;y++) hline(x0,x1,y,c); }

static void fill_disc(int cx,int cy,int r,uint32_t c)
{
    for(int dy=-r;dy<=r;dy++){
        int w=r*r-dy*dy,hw=0; while((hw+1)*(hw+1)<=w)hw++;
        hline(cx-hw,cx+hw,cy+dy,c);
    }
}
static void soft_disc(int cx,int cy,int r,uint32_t col,int steps)
{ for(int k=0;k<=steps;k++){ int rr=r-r*k/(steps+1);
    fill_disc(cx,cy,rr,_mix(C_BG_BOT,col,k+1,steps+1)); } }

static void wave(int ybase,int amp,int phase)
{
    for(int off=-1;off<=1;off++){
        int px0=0,py=0,first=1;
        for(int x=0;x<=SCR_W;x+=2){
            int y=ybase+off+amp*_isin((x*360/SCR_W)+phase)/256;
            int c=_mix(C_WAVE_L,C_WAVE_R,x,SCR_W);
            if(off) c=_mix(c,C_BG_BOT,1,2);
            if(!first){ int x0=px0,x1=x; if(x1<x0){int t=x0;x0=x1;x1=t;}
                if(y==py) hline(x0,x1,y,c);
                else { int ya=py<y?py:y,yb=py<y?y:py; for(int yy=ya;yy<=yb;yy++) px(x,yy,c); } }
            px0=x;py=y;first=0;
        }
    }
}

/* ================================================================= FONT == */
static FT_Library g_ft=NULL;
static FT_Face    g_face=NULL;
static char       g_fontpath[256]="";

static int font_open(void)
{
    const char *names[4];
    char p[256];
    names[0]="ui.ttf"; names[1]="xmb.ttf"; names[2]="Hardenne Black.ttf"; names[3]="Hardenne Regular.ttf";
    const char *dirs[3]={FONTDIR1,FONTDIR2,"/flash/launcher"};
    if (FT_Init_FreeType(&g_ft)) { fprintf(stderr,"FT_Init failed\n"); return -1; }
    for (int d=0;d<3;d++) for (int n=0;n<4;n++){
        snprintf(p,sizeof(p),"%s/%s",dirs[d],names[n]);
        if (access(p,R_OK)==0 && FT_New_Face(g_ft,p,0,&g_face)==0){
            strncpy(g_fontpath,p,sizeof(g_fontpath)-1);
            fprintf(stderr,"font: %s\n",p); return 0;
        }
    }
    fprintf(stderr,"font: none found (looked in %s,%s)\n",FONTDIR1,FONTDIR2);
    return -1;
}
static void font_close(void)
{ if(g_face){FT_Done_Face(g_face);g_face=NULL;} if(g_ft){FT_Done_FreeType(g_ft);g_ft=NULL;} }

/* text width at a given pixel size */
static int text_w(int size,const char *s)
{
    if(!g_face||!s) return 0;
    FT_Set_Pixel_Sizes(g_face,0,size);
    int w=0;
    for(const unsigned char *p=(const unsigned char*)s;*p;){
        uint32_t cp; int adv;
        if(*p<0x80){ cp=*p++; }
        else if((*p&0xE0)==0xC0){ cp=((*p&0x1F)<<6)|(p[1]&0x3F); p+=2; }
        else if((*p&0xF0)==0xE0){ cp=((*p&0x0F)<<12)|((p[1]&0x3F)<<6)|(p[2]&0x3F); p+=3; }
        else { cp=((*p&0x07)<<18)|((p[1]&0x3F)<<12)|((p[2]&0x3F)<<6)|(p[3]&0x3F); p+=4; }
        if(FT_Load_Char(g_face,cp,FT_LOAD_DEFAULT)) continue;
        adv=g_face->glyph->advance.x>>6;
        w+=adv?adv:(size/2);
    }
    return w;
}

/* draw UTF-8 text; align: 0 left,1 center,2 right; baseline helper */
static void text_draw(int size,const char *s,int x,int y,int maxw,int align,uint32_t col)
{
    if(!g_face||!s) return;
    FT_Set_Pixel_Sizes(g_face,0,size);
    int w=text_w(size,s);
    if(maxw>0 && w>maxw){
        /* naive truncation + ellipsis */
        char buf[128]; int n=strlen(s); if(n>120)n=120; memcpy(buf,s,n); buf[n]=0;
        while(n>1 && text_w(size,buf)>maxw){ n--; buf[n]=0; }
        if(n>=3){ buf[n-1]='.';buf[n-2]='.';buf[n-3]='.'; }
        s=buf; w=text_w(size,s);
    }
    int cx=x;
    if(align==1) cx=x-w/2; else if(align==2) cx=x-w;

    for(const unsigned char *p=(const unsigned char*)s;*p;){
        uint32_t cp;
        if(*p<0x80){ cp=*p++; }
        else if((*p&0xE0)==0xC0){ cp=((*p&0x1F)<<6)|(p[1]&0x3F); p+=2; }
        else if((*p&0xF0)==0xE0){ cp=((*p&0x0F)<<12)|((p[1]&0x3F)<<6)|(p[2]&0x3F); p+=3; }
        else { cp=((*p&0x07)<<18)|((p[1]&0x3F)<<12)|((p[2]&0x3F)<<6)|(p[3]&0x3F); p+=4; }
        if(FT_Load_Char(g_face,cp,FT_LOAD_RENDER)) { cx+=size/3; continue; }
        FT_GlyphSlot g=g_face->glyph;
        int bx=cx+g->bitmap_left, by=y - g->bitmap_top;
        for(int r=0;r<(int)g->bitmap.rows;r++)
            for(int c=0;c<(int)g->bitmap.width;c++){
                unsigned a=g->bitmap.buffer[r*g->bitmap.pitch+c];
                if(!a) continue;
                int X=bx+c,Y=by+r;
                if((unsigned)X<(unsigned)SCR_W && (unsigned)Y<(unsigned)SCR_H){
                    uint32_t *d=&g_buf[(size_t)Y*SCR_W+X];
                    *d=_mix(*d,col,a,255);
                }
            }
        cx+=g->advance.x>>6;
    }
}

/* ================================================================ COVERS == */
#define COVW 256
#define COVH 256
static uint16_t s_covpx[COVW*COVH];

static uint32_t rgb565(uint16_t v)
{ int r=(v>>11)&31,g=(v>>5)&63,b=v&31; return ((r<<3)|(r>>2))<<16 | (((g<<2)|(g>>4))<<8) | ((b<<3)|(b>>2)); }

static int cov_load_try(const char *stem,int *pw,int *ph)
{
    char p[300];
    const char *dirs[2]={COVDIR1,COVDIR2};
    char alt[128]; size_t j=0;
    for(size_t i=0;stem[i]&&j<sizeof(alt)-1;i++) alt[j++]= (stem[i]==' ')?'_':stem[i];
    alt[j]=0;
    for(int d=0;d<2;d++){
        const char *stems[2]={stem,alt};
        for(int s=0;s<2;s++){
            snprintf(p,sizeof(p),"%s/%s.565",dirs[d],stems[s]);
            FILE *f=fopen(p,"rb"); if(!f) continue;
            unsigned char h[8];
            if(fread(h,1,8,f)!=8 || h[0]!='C'||h[1]!='5'||h[2]!='6'||h[3]!='5'){fclose(f);continue;}
            int w=h[4]|(h[5]<<8),hh=h[6]|(h[7]<<8);
            if(w<1||hh<1||w>COVW||hh>COVH){fclose(f);continue;}
            if(fread(s_covpx,2,(size_t)w*hh,f)!=(size_t)w*hh){fclose(f);continue;}
            fclose(f); *pw=w;*ph=hh; return 1;
        }
    }
    return 0;
}
static void cover_draw(const char *stem,int fx0,int fy0,int fx1,int fy1)
{
    int fw=fx1-fx0,fh=fy1-fy0,cw,ch;
    if(fw<8||fh<8) return;
    if(!cov_load_try(stem,&cw,&ch)){
        for(int y=fy0;y<fy1;y++) hline(fx0,fx1,y,_mix(C_PANEL_T,C_PANEL_B,y-fy0,fh));
        return;
    }
    /* backdrop: dark coarse mosaic */
    int cols=12,rows=8;
    for(int cy=0;cy<rows;cy++)for(int cx=0;cx<cols;cx++){
        int rx0=fx0+cx*fw/cols,rx1=fx0+(cx+1)*fw/cols;
        int sx0=cx*cw/cols,sx1=cx*cw/cols+cw/cols+1;
        int sy0=cy*ch/rows,sy1=cy*ch/rows+ch/rows+1;
        unsigned rs=0,gs=0,bs=0,n=0;
        for(int yy=sy0;yy<sy1&&yy<ch;yy++)for(int xx=sx0;xx<sx1&&xx<cw;xx++){
            uint16_t v=s_covpx[(size_t)yy*cw+xx]; rs+=(v>>11)&31;gs+=(v>>5)&63;bs+=v&31;n++; }
        uint32_t col= n?((((rs/n)<<3)<<16)|(((gs/n)<<2)<<8)|((bs/n)<<3)):0;
        col=_mix(col,0,1,2);
        for(int y=cy*fh/rows;y<(cy+1)*fh/rows;y++) hline(fx0+cx*fw/cols,fx0+(cx+1)*fw/cols-1,fy0+y,col);
    }
    /* foreground contain */
    int dw,dh;
    if((long)cw*fh <= (long)ch*fw){ dw=fw; dh=(int)((long)ch*fw/cw);} else { dh=fh; dw=(int)((long)cw*fh/ch);}
    if(dw<1)dw=1; if(dh<1)dh=1;
    int ox=fx0+(fw-dw)/2, oy=fy0+(fh-dh)/2;
    for(int y=0;y<dh;y++){
        int sy=(int)((long)y*ch/dh);
        for(int x=0;x<dw;x++){ int sx=(int)((long)x*cw/dw);
            px(ox+x,oy+y,rgb565(s_covpx[(size_t)sy*cw+sx])); }
    }
}

/* ================================================================= INPUT == */
#define MAX_IN 8
static int g_in[MAX_IN]; static int g_nin=0;

static void in_open(void)
{
    g_nin=0;
    for(int i=0;i<16 && g_nin<MAX_IN;i++){
        char p[64]; struct stat st;
        snprintf(p,sizeof(p),"/dev/input/event%d",i);
        if(stat(p,&st)) continue;
        int fd=open(p,O_RDONLY|O_NONBLOCK);
        if(fd<0) continue;
        /* only devices that report keys */
        unsigned long kbf=0; if(ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(kbf)),&kbf)<0){close(fd);continue;}
        g_in[g_nin++]=fd;
        fprintf(stderr,"input: %s\n",p);
    }
}
static void in_close(void){ for(int i=0;i<g_nin;i++) close(g_in[i]); g_nin=0; }

enum { E_NONE=0,E_UP,E_DOWN,E_LEFT,E_RIGHT,E_OK,E_BACK,E_MENU };

static int key_event(int code,int val)
{
    if(val==0) return E_NONE;              /* release */
    switch(code){
    case BTN_DPAD_UP: case KEY_UP:   return E_UP;
    case BTN_DPAD_DOWN: case KEY_DOWN: return E_DOWN;
    case BTN_DPAD_LEFT: case KEY_LEFT: return E_LEFT;
    case BTN_DPAD_RIGHT: case KEY_RIGHT: return E_RIGHT;
    case BTN_SOUTH: case KEY_ENTER: case KEY_SPACE: return E_OK;
    case BTN_EAST:  case KEY_BACK: case KEY_ESC: return E_BACK;
    case BTN_MODE:  case BTN_START: case KEY_MENU: return E_MENU;
    default: return E_NONE;
    }
}

/* blocking wait for one launcher action */
static int in_wait(void)
{
    struct pollfd pfd[MAX_IN];
    for(int i=0;i<g_nin;i++){ pfd[i].fd=g_in[i]; pfd[i].events=POLLIN; }
    for(;;){
        int r=poll(pfd,g_nin,-1);
        if(r<=0){ if(errno==EINTR) continue; return E_NONE; }
        for(int i=0;i<g_nin;i++){
            if(pfd[i].revents & POLLIN){
                struct input_event ev;
                while(read(g_in[i],&ev,sizeof(ev))==sizeof(ev)){
                    if(ev.type==EV_KEY){ int a=key_event(ev.code,ev.value); if(a) return a; }
                    else if(ev.type==EV_ABS){
                        if(ev.code==ABS_HAT0Y) return ev.value<0?E_UP:(ev.value>0?E_DOWN:E_NONE);
                        if(ev.code==ABS_HAT0X) return ev.value<0?E_LEFT:(ev.value>0?E_RIGHT:E_NONE);
                    }
                }
            }
        }
    }
}

/* ================================================================== SCAN == */
static void scan_roms(void)
{
    DIR *root=opendir(ROMDIR);
    s_nsys=0;
    if(!root){ fprintf(stderr,"no %s\n",ROMDIR); return; }

    struct dirent *de;
    while((de=readdir(root)) && s_nsys<MAX_SYS){
        if(de->d_name[0]=='.') continue;
        char dp[300]; snprintf(dp,sizeof(dp),"%s/%s",ROMDIR,de->d_name);
        struct stat st; if(stat(dp,&st)||!S_ISDIR(st.st_mode)) continue;

        /* find a sysdef by dir name */
        const sysdef_t *def=NULL;
        for(int i=0;i<NSYS;i++) if(!strcasecmp(SYS[i].dir,de->d_name)){ def=&SYS[i]; break; }

        sys_t *S=&s_sys[s_nsys];
        memset(S,0,sizeof(*S));
        snprintf(S->dir,sizeof(S->dir),"%s",de->d_name);
        if(def){ snprintf(S->name,sizeof(S->name),"%s",def->name); snprintf(S->core,sizeof(S->core),"%s",def->core); }
        else   { snprintf(S->name,sizeof(S->name),"%s",de->d_name); S->core[0]=0; }

        DIR *d=opendir(dp); if(!d) continue;
        struct dirent *e;
        while((e=readdir(d)) && S->n<MAX_GAMES){
            if(e->d_name[0]=='.') continue;
            bool ok=false;
            if(def){ /* accept any file (dir is known) unless it's clearly not a rom */
                if(!_ends(e->d_name,".txt")&&!_ends(e->d_name,".jpg")&&!_ends(e->d_name,".png")
                   &&!_ends(e->d_name,".565")&&!_ends(e->d_name,".srm")&&!_ends(e->d_name,".state")) ok=true;
            } else {
                for(int x=0;EXT[x];x++) if(_ends(e->d_name,EXT[x])){ok=true;break;}
            }
            if(!ok) continue;
            char fp[300]; snprintf(fp,sizeof(fp),"%s/%s",dp,e->d_name);
            struct stat fs; if(stat(fp,&fs)||!S_ISREG(fs.st_mode)) continue;
            game_t *G=&S->g[S->n];
            snprintf(G->path,sizeof(G->path),"%s",fp);
            /* label = stem, underscores -> spaces */
            snprintf(G->label,sizeof(G->label),"%s",e->d_name);
            char *dot=strrchr(G->label,'.'); if(dot)*dot=0;
            for(char *p=G->label;*p;p++) if(*p=='_')*p=' ';
            S->n++;
        }
        closedir(d);
        /* sort games by label */
        for(int a=1;a<S->n;a++){ game_t t=S->g[a]; int b=a-1;
            while(b>=0 && strcasecmp(S->g[b].label,t.label)>0){ S->g[b+1]=S->g[b]; b--; }
            S->g[b+1]=t; }
        if(S->n>0) s_nsys++;
    }
    closedir(root);

    /* simple insertion sort by name */
    for(int i=1;i<s_nsys;i++){ sys_t t=s_sys[i]; int j=i-1;
        while(j>=0 && strcasecmp(s_sys[j].name,t.name)>0){ s_sys[j+1]=s_sys[j]; j--; }
        s_sys[j+1]=t; }
    fprintf(stderr,"scan: %d systems\n",s_nsys);
}

/* =============================================================== LAUNCH == */
static int run_game(const sys_t *S,const game_t *G)
{
    char core[300];
    if(S->core[0]) snprintf(core,sizeof(core),"%s/%s",COREDIR,S->core);
    else return -1;

    /* close our inputs so RA has exclusive access */
    in_close();
    fb_close();

    pid_t pid=fork();
    if(pid==0){
        setsid();
        execl(RABIN,"retroarch","-L",core,G->path,(char*)NULL);
        /* fallback to PATH retroarch */
        execlp("retroarch","retroarch","-L",core,G->path,(char*)NULL);
        _exit(127);
    }
    int st=0; if(pid>0) waitpid(pid,&st,0);

    /* re-init devices (RA may have changed fb mode) */
    fb_open();
    in_open();
    return 0;
}

/* ================================================================== DRAW == */
static const char *hint_ru = "OK - Играть    Меню - Настройки    Назад - Выход";
static const char *hint_en = "OK - Play    Menu - Settings    Back - Exit";

static void draw_bg(void)
{
    for(int y=0;y<SCR_H;y++) hline(0,SCR_W-1,y,_mix(C_BG_TOP,C_BG_BOT,y,SCR_H));
    wave(96,26,0);
    wave(96,20,60);
}

static void sel_box(int x0,int y0,int x1,int y1)
{
    uint32_t c=_mix(C_BAR_M,0xFFFFFF,120,255);
    for(int y=y0;y<=y1;y++) hline(x0,x1,y,_mix(g_buf[(size_t)y*SCR_W+((x0+x1)/2)],c,60,255));
    uint32_t br=_mix(C_BAR_M,0xFFFFFF,150,255);
    for(int x=x0;x<=x1;x++){ px(x,y0,br); px(x,y1,br); }
    for(int y=y0;y<=y1;y++){ px(x0,y,br); px(x1,y,br); }
}

static void draw_cat_bar(void)
{
    int cx=SCR_W/2, slot=74;
    for(int i=0;i<s_nsys;i++){
        int x=cx+(i-s_syssel)*slot;
        if(x<-40||x>SCR_W+40) continue;
        int sel=(i==s_syssel);
        if(sel) soft_disc(x,26,22,C_GLOW,6);
        text_draw(sel?13:11,s_sys[i].name,x,34,slot-6,1,sel?C_CAT_SEL:C_CAT_DIM);
    }
}

static void draw_list(void)
{
    sys_t *S=&s_sys[s_syssel];
    int x0=10, lh=15, y0=52;
    int right = s_cov ? 196 : SCR_W-8;
    int rows=(SCR_H-18-y0)/lh; if(rows<1)rows=1;
    if(s_top> S->n-rows) s_top=S->n-rows; if(s_top<0)s_top=0;

    for(int i=s_top;i<S->n && i<s_top+rows;i++){
        int iy=y0+(i-s_top)*lh;
        int sel=(i==s_gsel);
        if(sel) sel_box(x0-4,iy-1,right,iy+lh-3);
        text_draw(11,S->g[i].label,x0,iy+lh-4,right-x0-4,0,sel?C_ITEM_SEL:C_ITEM_DIM);
    }
    if(s_top>0) text_draw(10,"...",x0,y0-2,20,0,C_HINT);
    if(s_top+rows<S->n) text_draw(10,"...",x0,y0+rows*lh+2,20,0,C_HINT);

    if(s_cov && S->n>0)
        cover_draw(S->g[s_gsel].label,204,50,SCR_W-8,50+(SCR_W-8-204));
}

static void draw_hints(void)
{
    const char *h= s_lang==0?hint_ru:hint_en;
    text_draw(9,h,SCR_W-8,SCR_H-6,SCR_W-16,2,C_HINT);
}

static void draw(void)
{
    draw_bg();
    draw_cat_bar();
    if(s_nsys==0) text_draw(12,"ROMs: /storage/roms",SCR_W/2,SCR_H/2,SCR_W-20,1,C_ITEM_DIM);
    else draw_list();
    draw_hints();
    fb_flip();
}

/* ================================================================ CONFIG == */
static void cfg_load(void)
{
    FILE *f=fopen(CFGPATH,"r"); if(!f) return;
    char l[64];
    while(fgets(l,sizeof(l),f)){
        if(!strncmp(l,"theme=",6)) s_theme=atoi(l+6);
        else if(!strncmp(l,"covers=",7)) s_cov=atoi(l+7);
        else if(!strncmp(l,"lang=",5)) s_lang=atoi(l+5);
    }
    fclose(f);
}
static void cfg_save(void)
{
    FILE *f=fopen(CFGPATH,"w"); if(!f) return;
    fprintf(f,"theme=%d\ncovers=%d\nlang=%d\n",s_theme,s_cov,s_lang); fclose(f);
}
static void apply_theme(int t){ if(t<0||t>=NTH)t=0; s_theme=t;
    C_BG_TOP=TH[t].top;C_BG_BOT=TH[t].bot;C_WAVE_L=TH[t].wl;C_WAVE_R=TH[t].wr;
    C_GLOW=TH[t].glow;C_BAR_M=TH[t].bar;
    C_PANEL_T=_mix(C_BG_TOP,0xFFFFFF,40,255);
    C_PANEL_B=_mix(C_BG_BOT,0xFFFFFF,40,255);
    C_PANEL_EDGE=_mix(C_BG_BOT,0xFFFFFF,110,255);
}

/* ================================================================ SETTINGS */
static void settings_menu(void)
{
    for(;;){
        draw();
        int px0=40,py0=36,px1=SCR_W-40,py1=SCR_H-36;
        for(int y=py0;y<=py1;y++) hline(px0,px1,y,_mix(C_PANEL_T,C_PANEL_B,y-py0,py1-py0));
        for(int x=px0;x<=px1;x++){px(x,py0,C_PANEL_EDGE);px(x,py1,C_PANEL_EDGE);}
        for(int y=py0;y<=py1;y++){px(px0,y,C_PANEL_EDGE);px(px1,y,C_PANEL_EDGE);}
        const char *ti = s_lang==0?"Настройки":"Settings";
        text_draw(13,ti,px0+10,py0+18,px1-px0-20,0,C_CAT_SEL);
        char b0[64],b1[64],b2[64],b3[64];
        snprintf(b0,sizeof(b0), (s_lang==0?"Обложки: %s":"Covers: %s"), s_cov?(s_lang==0?"Вкл":"On"):(s_lang==0?"Выкл":"Off"));
        snprintf(b1,sizeof(b1), (s_lang==0?"Тема: %d":"Theme: %d"), s_theme+1);
        snprintf(b2,sizeof(b2), (s_lang==0?"Язык: %s":"Language: %s"), s_lang==0?"Русский":"English");
        snprintf(b3,sizeof(b3), (s_lang==0?"Выход":"Exit"));
        const char *it[4]={b0,b1,b2,b3};
        for(int i=0;i<4;i++){ int y=py0+40+i*20;
            text_draw(11,it[i],px0+16,y+12,px1-px0-30,0,i==0?C_ITEM_SEL:C_ITEM_DIM);
            if(i==0) sel_box(px0+8,y+1,px1-8,y+16);
        }
        text_draw(9, s_lang==0?"↑↓ выбор  OK изменить  Назад закрыть":"up/down sel  OK change  Back close",
                  px0+16,py1-8,px1-px0-32,0,C_HINT);
        fb_flip();

        int a=in_wait();
        if(a==E_BACK||a==E_MENU) { cfg_save(); return; }
        if(a==E_LEFT||a==E_RIGHT||a==E_OK){
            if(a==E_OK||a==E_RIGHT){ if(s_cov) { s_cov=0; } else { s_cov=1; } }
            cfg_save();
        }
        /* up/down on this single-item page: cycle theme */
        if(a==E_UP||a==E_DOWN){ s_theme=(s_theme+(a==E_DOWN?1:NTH-1))%NTH; apply_theme(s_theme); cfg_save(); }
        if(a==E_LEFT){ /* unused */ }
    }
}

/* =================================================================== MAIN */
#ifndef HOST_TEST
int main(int argc,char **argv)
{
    (void)argc;(void)argv;
    signal(SIGPIPE,SIG_IGN);

    cfg_load();
    apply_theme(s_theme);

    if (fb_open()) { fprintf(stderr,"launcher: no fb\n"); return 1; }
    font_open();                 /* soft-fail: text just won't render */
    in_open();
    scan_roms();

    for(;;){
        if(s_nsys<=0){ draw(); /* still show "no roms" */ }
        draw();
        int a=in_wait();
        if(a==E_NONE) continue;

        sys_t *S = s_nsys>0 ? &s_sys[s_syssel] : NULL;

        if(a==E_LEFT && s_nsys>0){ s_syssel=(s_syssel+s_nsys-1)%s_nsys; s_gsel=0; s_top=0; }
        else if(a==E_RIGHT && s_nsys>0){ s_syssel=(s_syssel+1)%s_nsys; s_gsel=0; s_top=0; }
        else if(a==E_UP && S && S->n>0){ if(--s_gsel<0) s_gsel=S->n-1; }
        else if(a==E_DOWN && S && S->n>0){ if(++s_gsel>=S->n) s_gsel=0; }
        else if(a==E_OK && S && S->n>0){ run_game(S,&S->g[s_gsel]); }
        else if(a==E_MENU){ settings_menu(); }
        else if(a==E_BACK){ break; }

        /* keep the selected row visible */
        int rows=(SCR_H-18-52)/15; if(rows<1)rows=1;
        if(S){ if(s_gsel<s_top) s_top=s_gsel;
               if(s_gsel>=s_top+rows) s_top=s_gsel-rows+1; }
    }

    in_close();
    font_close();
    fb_close();
    return 0;
}
#else  /* HOST_TEST: render a few screens to /tmp/launcher.ppm */
int main(void)
{
    const char *want_sys=getenv("SYS"), *want_g=getenv("GAME");
    cfg_save(); cfg_load(); apply_theme(s_theme);
    fb_open(); font_open(); scan_roms();
    fprintf(stderr,"host: %d systems\n",s_nsys);
    for(int i=0;i<s_nsys;i++) fprintf(stderr,"  %-12s n=%d\n",s_sys[i].name,s_sys[i].n);
    s_syssel=0; s_gsel=0; s_top=0;
    for(int i=0;i<s_nsys;i++) if(want_sys && !strcasecmp(s_sys[i].name,want_sys)) s_syssel=i;
    if(want_g) for(int j=0;j<s_sys[s_syssel].n;j++)
        if(!strcasecmp(s_sys[s_syssel].g[j].label,want_g)) s_gsel=j;
    if(s_cov){
        for(int j=0;j<s_sys[s_syssel].n;j++){ int w,h; if(cov_load_try(s_sys[s_syssel].g[j].label,&w,&h)){ s_gsel=j; break; } }
    }
    draw();
    fprintf(stderr,"wrote /tmp/launcher.ppm (sys=%s game=%s)\n",
            s_sys[s_syssel].name, s_sys[s_syssel].g[s_gsel].label);
    return 0;
}
#endif
