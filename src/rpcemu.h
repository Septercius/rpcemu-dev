/*RPCemu v0.5 by Tom Walker
  Main header file*/

#ifndef _rpc_h
#define _rpc_h

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GRAPHICS_TYPE GFX_AUTODETECT_WINDOWED

/*This moves the calls to blit() and stretch_blit() to a seperate thread. It
  gives a large speedup on a dual-core processor when lots of screen data is
  being updated (eg a full 800x600 screen), and improves the sound stability a
  bit. Not sure how it performs on a single core processor.
  This alters vidc20.c a little - when the rest of drawscr() finishes, it sets a
  flag instead of blitting. This is tested by blitterthread(), which must be
  called regularly. If a thread is not created in the platform specific file,
  then no blits happen, and the emulator will hang due to the synchronisation in
  place.*/
#define BLITTER_THREAD
  
/*ARM*/
extern uint32_t *usrregs[16],userregs[17],superregs[17],fiqregs[17],irqregs[17],abortregs[17],undefregs[17],systemregs[17];
extern uint32_t spsr[16];
extern uint32_t armregs[17];
extern int armirq; //,armfiq;
#define PC ((armregs[15])&r15mask)
extern uint32_t ins,output;
extern int r15mask;
extern uint32_t mode;
extern int irq;

void resetarm(void);
void execarm(int cycles);
void dumpregs(void);

extern int databort,prefabort;
extern int prog32;
  //unsigned long *ram,*ram2,*rom,*vram;
  //unsigned char *ramb,*ramb2,*romb,*vramb;
  //unsigned char *dirtybuffer;

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
#define translateaddress(addr,rw) ((!((addr)&0xFC000000) && !(tlbcache[((addr)>>12)&0x3FFF]&0xFFF))?(tlbcache[(addr)>>12]|((addr)&0xFFF)):translateaddress2(addr,rw))

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

/* mem.c */
//uint32_t readmeml(uint32_t addr);
uint32_t readmemfl(uint32_t addr);
uint32_t readmemfb(uint32_t addr);
//void writememl(uint32_t addr, uint32_t val);
void writememb(uint32_t addr, uint8_t val);
uint32_t readmemfb(uint32_t addr);
//uint32_t translateaddress(uint32_t addr, int rw);

//uint32_t mem_getphys(uint32_t addr);
//uint32_t readmeml_phys(uint32_t addr);

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

/* 82c711.c */
void callbackfdc(void);
uint8_t read82c711(uint32_t addr);
uint8_t readfdcdma(uint32_t addr);
void writefdcdma(uint32_t addr, uint8_t val);
void write82c711(uint32_t addr, uint32_t val);

/* cmos.c */
void reseti2c(void);
void cmosi2cchange(int nuclock, int nudata);

/* vidc20.c */
int getxs(void);
int getys(void);
void resetbuffer(void);
void writevidc20(uint32_t val);

/* iomd.c */
void resetiomd(void);
uint32_t readiomd(uint32_t addr);
void writeiomd(uint32_t addr, uint32_t val);
uint8_t readmb(void);

char HOSTFS_ROOT[512];

char discname[2][260];
int drawscre;

/*Sound*/
int soundenabled;
int soundbufferfull;
void updatesoundirq();
void updatesoundbuffer();
uint32_t soundaddr[4];
int samplefreq;
int soundinited,soundlatch,soundcount;

/*Generic*/
int lastinscount;
int infocus;
#endif
