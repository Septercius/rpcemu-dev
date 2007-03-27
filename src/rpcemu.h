/*RPCemu v0.5 by Tom Walker
  Main header file*/

#ifndef _rpc_h
#define _rpc_h

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GRAPHICS_TYPE GFX_AUTODETECT_WINDOWED

/*This determines whether RPCemu can use hardware to blit and scale the display.
  If this is disabled then modes lower than 640x480 can look odd, and the system
  is slower. However, this must be commented out on some ports (Linux)*/
#ifndef __unix
#define HARDWAREBLIT
#endif

/*This moves the calls to blit() and stretch_blit() to a seperate thread. It
  gives a large speedup on a dual-core processor when lots of screen data is
  being updated (eg a full 800x600 screen), and improves the sound stability a
  bit. Not sure how it performs on a single core processor.
  This alters vidc20.c a little - when the rest of drawscr() finishes, it sets a
  flag instead of blitting. This is tested by blitterthread(), which must be
  called regularly. If a thread is not created in the platform specific file,
  then no blits happen, and the emulator will hang due to the synchronisation in
  place.
  In Windows, on many systems, this _must_ be enabled. Otherwise mouse & keyboard
  response will be appallingly bad.*/
#ifndef __unix
#define BLITTER_THREAD
#endif

/*This makes the RISC OS mouse pointer follow the host pointer exactly. Useful
  for Linux port, however use mouse capturing if possible - mousehack has some
  bugs*/
#ifdef __unix
#define mousehackena 1
#else
#define mousehackena 0
#endif

#define mousehack (mousehackena&&mousehackon)

int mousehackon;
//#define PREFETCH

/*ARM*/
extern uint32_t *usrregs[16],userregs[17],superregs[17],fiqregs[17],irqregs[17],abortregs[17],undefregs[17],systemregs[17];
extern uint32_t spsr[16];
extern uint32_t armregs[17];
extern int armirq; //,armfiq;
extern int cpsr;
#ifdef PREFETCH
#define PC (armregs[15]&r15mask)
#else
#define PC ((armregs[15]-8)&r15mask)
#endif
extern uint32_t ins,output;
extern int r15mask;
extern uint32_t mode;
extern int irq;
unsigned char flaglookup[16][16];
void resetarm(void);
void execarm(int cycles);
void dumpregs(void);

extern int databort,prefabort;
extern int prog32;

uint32_t oldpc,oldpc2,oldpc3;
  //unsigned long *ram,*ram2,*rom,*vram;
  //unsigned char *ramb,*ramb2,*romb,*vramb;
  //unsigned char *dirtybuffer;

/* mem.c */
uint32_t readmemfl(uint32_t addr);
uint32_t readmemfb(uint32_t addr);
void writememb(uint32_t addr, uint8_t val);
void writememfl(uint32_t addr, uint32_t val);
uint32_t readmemfb(uint32_t addr);
void clearmemcache();
void initmem(void);
void reallocmem(int ramsize);

extern uint32_t raddrl[256];
extern uint32_t *raddrl2[256];
//#define readmeml(a) readmemfl(a)

#define readmeml(a) ((((a)&0xFFFFF000)==raddrl[((a)>>12)&0xFF])?raddrl2[((a)>>12)&0xFF][(a)>>2]:readmemfl(a))
//#define readmeml(a) ((((a)&0xFFFFF000)==raddrl)?raddrl2[((a)&0xFFC)>>2]:readmemfl(a))

#define readmemb(a) ((((a)&0xFFFFF000)==raddrl[((a)>>12)&0xFF])?((unsigned char *)raddrl2[((a)>>12)&0xFF])[(a)]:readmemfb(a))

extern uint32_t waddrl;
extern uint32_t *waddrl2;
//#define writememl(a,v) writememfl(a,v)
#define writememl(a,v) if (((a)&0xFFFFF000)==waddrl) { waddrl2[((a)&0xFFC)>>2]=v; } else { writememfl(a,v); }

extern uint32_t *ram,*ram2,*rom,*vram;
extern uint8_t *ramb,*romb,*vramb;
extern uint8_t dirtybuffer[512];

uint32_t tlbcache[16384];
#define translateaddress(addr,rw,prefetch) ((!((addr)&0xFC000000) && !(tlbcache[((addr)>>12)&0x3FFF]&0xFFF))?(tlbcache[(addr)>>12]|((addr)&0xFFF)):translateaddress2(addr,rw,prefetch))

extern int mmu,memmode;

/*IOMD*/
struct iomd
{
        unsigned char stata,statb,statc,statd,state,statf;
        unsigned char maska,maskb,maskc,maskd,maske,maskf;
        unsigned char romcr0,romcr1;
        uint32_t vidstart,vidend,vidcur,vidinit;
        int t0l,t1l,t0c,t1c,t0r,t1r;
        unsigned char ctrl;
        unsigned char vidcr;
        unsigned char sndstat;
        unsigned char keydat;
        unsigned char msdat;
        int mousex,mousey;
} iomd;

void gentimerirq();
int delaygenirqleft, delaygenirq;

int i2cclock,i2cdata;

int kcallback,mcallback;

uint32_t cinit;
int fdccallback;
int motoron;

char exname[512];

int idecallback;

/*Config*/
int vrammask;
int model;
int rammask;
int stretchmode;

extern uint32_t soundaddr[4];

extern uint32_t inscount;
int rinscount;
int cyccount;

/* arm.c */
void updatemode(uint32_t m);

/* rpc-[linux|win].c */
void error(const char *format, ...);
void rpclog(const char *format, ...);
void updatewindowsize(uint32_t x, uint32_t y);

void updateirqs(void);

/* ide.c */
void writeide(uint16_t addr, uint8_t val);
void writeidew(uint16_t val);
uint8_t readide(uint16_t addr);
uint16_t readidew(void);
void callbackide(void);
void resetide(void);

/* cp15.c */
void writecp15(uint32_t addr, uint32_t val);
uint32_t readcp15(uint32_t addr);
void resetcp15(void);
uint32_t *getpccache(uint32_t addr);
uint32_t translateaddress2(uint32_t addr, int rw, int prefetch);

/* keyboard.c */
void resetkeyboard(void);
void keycallback(void);
void mscallback(void);
void writekbd(uint8_t v);
void writekbdenable(int v);
void writems(unsigned char v);
void writemsenable(int v);
unsigned char getkeyboardstat(void);
unsigned char readkeyboarddata(void);
unsigned char getmousestat(void);
unsigned char readmousedata(void);
void pollmouse();
void pollkeyboard();

void doosmouse();
void setmouseparams(uint32_t a);
void getunbufmouse(uint32_t a);
void setmousepos(uint32_t a);
void osbyte106(uint32_t a);
void setpointer(uint32_t a);
void getosmouse();
void getmousepos(int *x, int *y);


/* 82c711.c */
void reset82c711();
void callbackfdc(void);
uint8_t read82c711(uint32_t addr);
uint8_t readfdcdma(uint32_t addr);
void writefdcdma(uint32_t addr, uint8_t val);
void write82c711(uint32_t addr, uint32_t val);
void loadadf(char *fn, int drive);
void saveadf(char *fn, int drive);

/* cmos.c */
void loadcmos();
void savecmos();
void reseti2c(void);
void cmosi2cchange(int nuclock, int nudata);
void cmostick();

/* vidc20.c */
void initvideo();
void closevideo();
void blitterthread();
int getxs(void);
int getys(void);
void resetbuffer(void);
void writevidc20(uint32_t val);
void drawscr();
void togglefullscreen(int fs);
int refresh;
int skipblits;

/* iomd.c */
void resetiomd(void);
void endiomd();
uint32_t readiomd(uint32_t addr);
void writeiomd(uint32_t addr, uint32_t val);
uint8_t readmb(void);
void iomdvsync();

char HOSTFS_ROOT[512];

char discname[2][260];
int drawscre;

/*Sound*/
void initsound();
void closesound();
void changesamplefreq();
int soundenabled;
int soundbufferfull;
void updatesoundirq();
void updatesoundbuffer();
int getbufferlen();
uint32_t soundaddr[4];
int samplefreq;
int soundinited,soundlatch,soundcount;

/*Generic*/
int lastinscount;
int infocus;

/*FPA*/
void resetfpa();
void dumpfpa();
void fpaopcode(uint32_t opcode);


int loadroms();

/*rpcemu.c*/
int startrpcemu();
void execrpcemu();
void endrpcemu();

int quited;
#endif
