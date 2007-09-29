#ifndef __MEM__
#define __MEM__

#include <stdint.h>

extern int loadroms();

extern uint32_t readmemfl(uint32_t addr);
extern uint32_t readmemfb(uint32_t addr);
extern void writememfb(uint32_t addr, uint8_t val);
extern void writememfl(uint32_t addr, uint32_t val);
extern uint32_t readmemfb(uint32_t addr);
extern void clearmemcache();
extern void initmem(void);
extern void reallocmem(int ramsize);

extern uint32_t raddrl[256];
extern uint32_t *raddrl2[256];

extern unsigned long *vraddrl;
extern uint32_t vraddrls[1024],vraddrphys[1024];
extern int vraddrlpos;
//#define readmeml(a) readmemfl(a)

#define readmeml(a) ((vraddrl[(a)>>12]&1)?readmemfl(a):*(uint32_t *)(/*(int32_t)*/(a)+(vraddrl[(a)>>12])))
#ifdef _RPCEMU_BIG_ENDIAN
	#define readmemb(a) ((vraddrl[(a)>>12]&1)?readmemfb(a):*(unsigned char *)(((a)^3)+(vraddrl[(a)>>12])))
#else
	#define readmemb(a) ((vraddrl[(a)>>12]&1)?readmemfb(a):*(unsigned char *)((a)+(vraddrl[(a)>>12])))
#endif

//#define readmeml(a) ((((a)>>12)==raddrl[((a)>>12)&0xFF])?raddrl2[((a)>>12)&0xFF][(a)>>2]:readmemfl(a))
//#define readmeml(a) ((((a)&0xFFFFF000)==raddrl)?raddrl2[((a)&0xFFC)>>2]:readmemfl(a))

//#define readmemb(a) ((((a)>>12)==raddrl[((a)>>12)&0xFF])?((unsigned char *)raddrl2[((a)>>12)&0xFF])[(a)]:readmemfb(a))

extern uint32_t waddrl;
extern uint32_t *waddrl2;
extern uint32_t waddrbl;
extern uint32_t *waddrbl2;

extern unsigned long *vwaddrl;
extern uint32_t vwaddrls[1024],vwaddrphys[1024];
extern int vwaddrlpos;

//uint8_t pagedirty[0x1000];
//#define writememb(a,v) writememfb(a,v)
#define HASH(l) (((l)>>2)&0x7FFF)
#define writememl(a,v) if (vwaddrl[(a)>>12]&3) writememfl(a,v); else { *(uint32_t *)(/*(int32_t)*/(a)+vwaddrl[(a)>>12])=v; }
#ifdef _RPCEMU_BIG_ENDIAN
	#define writememb(a,v) if (vwaddrl[(a)>>12]&3) writememfb(a,v); else { *(unsigned char *)(((a)^3)+vwaddrl[(a)>>12])=v; }
#else
	#define writememb(a,v) if (vwaddrl[(a)>>12]&3) writememfb(a,v); else { *(unsigned char *)((a)+vwaddrl[(a)>>12])=v; }
#endif
//#define writememl(a,v) if (((a)>>12)==waddrl) { waddrl2[((a)&0xFFC)>>2]=v; /*pagedirty[HASH(a)]=1;*/ } else { writememfl(a,v); }
//#define writememb(a,v) if (((a)>>12)==waddrbl) { ((unsigned char *)waddrbl2)[(a)&0xFFF]=v; /*pagedirty[HASH(a)]=1;*/ } else { writememfb(a,v); }

extern uint32_t *ram,*ram2,*rom,*vram;
extern uint8_t *ramb,*romb,*vramb;

extern uint32_t tlbcache[0x100000];
#define translateaddress(addr,rw,prefetch) ((/*!((addr)&0xFC000000) && */!(tlbcache[((addr)>>12)/*&0x3FFF*/]&0xFFF))?(tlbcache[(addr)>>12]|((addr)&0xFFF)):translateaddress2(addr,rw,prefetch))

extern int mmu,memmode;

extern int pcisrom;

#endif //__MEM__
