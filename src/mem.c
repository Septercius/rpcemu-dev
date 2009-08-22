/*RPCemu v0.6 by Tom Walker
  Memory handling*/
#include <assert.h>

#include "rpcemu.h"
#include "vidc20.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"
#include "cp15.h"
#include "superio.h"
#include "podules.h"
#include "fdc.h"

/* References -
   Acorn Risc PC - Technical Reference Manual
*/

uint32_t *ram = NULL, *ram2 = NULL, *rom = NULL, *vram = NULL;
uint8_t *ramb = NULL, *ramb2 = NULL, *romb = NULL, *vramb = NULL;

int mmu = 0, memmode = 0;

static uint32_t readmemcache = 0,readmemcache2 = 0;
static uint32_t writememcache = 0,writememcache2 = 0;
static uint32_t writemembcache = 0,writemembcache2 = 0;

void clearmemcache(void)
{
        writememcache=0xFFFFFFFF;
        readmemcache=0xFFFFFFFF;
}

static int vraddrlpos, vwaddrlpos;

/**
 * Initialise memory (called only once on program startup)
 */
void mem_init(void)
{
	rom  = malloc(ROMSIZE);
	vram = malloc(8 * 1024 * 1024); /*8 meg VRAM!*/
	romb  = (unsigned char *) rom;
	vramb = (unsigned char *) vram;
}

/**
 * Initialise/reset RAM (called on startup and emulated machine reset)
 *
 * @param ramsize Amount of RAM in bytes for each of two RAM banks
 *                (i.e. half the desired RAM)
 */
void mem_reset(uint32_t ramsize)
{
	assert(ramsize >= 2 * 1024 * 1024); /* At least 2MB per bank */
	assert(ramsize <= 64 * 1024 * 1024); /* At most 64MB per bank */
	assert(((ramsize - 1) & ramsize) == 0); /* Must be a power of 2 */

	ram  = realloc(ram, ramsize);
	ram2 = realloc(ram2, ramsize);
	ramb  = (unsigned char *) ram;
	ramb2 = (unsigned char *) ram2;
	memset(ram, 0, ramsize);
	memset(ram2, 0, ramsize);

	vraddrlpos = vwaddrlpos = 0;
}

#define vradd(a,v,f,p) if (vraddrls[vraddrlpos]!=0xFFFFFFFF) vraddrl[vraddrls[vraddrlpos]]=0xFFFFFFFF; \
                       vraddrls[vraddrlpos]=a>>12; \
                       vraddrl[a>>12]=(unsigned long)(v);/*|(f);*/ \
                       vraddrphys[vraddrlpos]=p; \
                       vraddrlpos=(vraddrlpos+1)&1023;
                       
#define vwadd(a,v,f,p) cacheclearpage(a>>12); /*Invalidate all code blocks on this page, so that any blocks on this page are forced to be recompiled*/ \
                       if (vwaddrls[vwaddrlpos]!=0xFFFFFFFF) vwaddrl[vwaddrls[vwaddrlpos]]=0xFFFFFFFF; \
                       vwaddrls[vwaddrlpos]=a>>12; \
                       vwaddrl[a>>12]=(unsigned long)(v);/*|(f);*/ \
                       vwaddrphys[vwaddrlpos]=p; \
                       vwaddrlpos=(vwaddrlpos+1)&1023;

uint32_t readmemfl(uint32_t addr)
{
        uint32_t addr2;

        if (mmu)
        {
                addr2=addr;
                if ((addr>>12)==readmemcache)
                {
                        addr=(addr&0xFFF)+readmemcache2;
                }
                else
                {
                        readmemcache=addr>>12;
                        armirq&=~0x40;
                        addr=translateaddress(addr,0,0);
                        if (armirq&0x40)
                        {
                                vraddrl[addr2>>12]=readmemcache=0xFFFFFFFF;
                                return 0;
                        }
                        readmemcache2=addr&0xFFFFF000;
                }
                        switch (readmemcache2&0x1F000000)
                        {
                                case 0x00000000: /*ROM*/
                                vradd(addr2,&rom[((readmemcache2&0x7FF000)-(long)(addr2&~0xFFF))>>2],2,readmemcache2);
                                return *(uint32_t *)((vraddrl[addr2>>12]&~3)+(addr2&~3));
                                
                                case 0x02000000: /*VRAM*/
                                dirtybuffer[(addr & config.vrammask) >> 12] = 2;
                                vradd(addr2,&vram[((readmemcache2&0x7FF000)-(long)(addr2&~0xFFF))>>2],0,readmemcache2);
                                return *(uint32_t *)(vraddrl[addr2>>12]+(addr2&~3));
                                
                                case 0x10000000: /*SIMM 0 bank 0*/
                                case 0x11000000:
                                case 0x12000000:
                                case 0x13000000:
                                vradd(addr2, &ram[((readmemcache2 & config.rammask) - (long)(addr2 & ~0xFFF))>>2], 0, readmemcache2);
                                return *(uint32_t *)(vraddrl[addr2>>12]+(addr2&~3));
                                
                                case 0x14000000: /*SIMM 0 bank 1*/
                                case 0x15000000:
                                case 0x16000000:
                                case 0x17000000:
                                vradd(addr2, &ram2[((readmemcache2 & config.rammask) - (long)(addr2 & ~0xFFF))>>2], 0, readmemcache2);
                                return *(uint32_t *)(vraddrl[addr2>>12]+(addr2&~3));
                                
                                default:
                                vraddrl[addr2>>12]=0xFFFFFFFF;
                        }
//                }
        }
        else// if (0)
        {
                switch (addr&0x1F000000)
                {
                       case 0x00000000: /*ROM*/
//                       vradd(addr,&rom[((addr&0x7FF000)-(addr&~0xFFF))>>2],2,addr);
                       break;
                       case 0x02000000: /*VRAM*/
                       vradd(addr,&vram[((addr&0x7FF000)-(long)(addr&~0xFFF))>>2],0,addr);
                       break;
                       case 0x10000000: /*SIMM 0 bank 0*/
                       case 0x11000000:
                       case 0x12000000:
                       case 0x13000000:
                       vradd(addr, &ram[((addr & config.rammask & ~0xFFF) - (long)(addr & ~0xFFF)) >> 2], 0, addr);
                       break;
                       case 0x14000000: /*SIMM 0 bank 1*/
                       case 0x15000000:
                       case 0x16000000:
                       case 0x17000000:
                       vradd(addr, &ram2[((addr & config.rammask & ~0xFFF) - (long)(addr & ~0xFFF)) >> 2], 0, addr);
                       break;
                       default:
                       vraddrl[addr>>12]=0xFFFFFFFF;
                }
        }
        if ((addr&0x1C000000)==0x10000000)
           return ram[(addr & config.rammask) >> 2];
        switch (addr&0x1F000000)
        {
            case 0x00000000: /*ROM*/
                return rom[(addr&0x7FFFFF)>>2];
            case 0x01000000: /*Extension ROM*/
                return 0;
            case 0x02000000: /*VRAM*/
                if (!config.vrammask || config.model == CPUModel_ARM7500)
                  return 0xFFFFFFFF;
                return vram[(addr & config.vrammask) >> 2];

            case 0x03000000: /*IO*/
                if (!(addr&0xC00000))
                {
                        /* 03000000 - 033fffff */
                        uint32_t bank = (addr >> 16) & 7;

                        switch (bank)
                        {
                        case 0:
                                /* IOMD Registers */
                                return readiomd(addr);
                        case 1:
                        case 2:
                                if (addr==0x3310000)
                                   return mouse_buttons_read();
                                if (addr >= 0x3010000 && addr < 0x3012000) {
                                        /* SuperIO */
                                        if ((addr&0xFFC)==0x7C0)
                                        {
                                                ideboard=0;
                                                return readidew();
                                        }
                                        return superio_read(addr);
                                }
                                break;
                        case 4:
                                /* Podule space 0, 1, 2, 3 */
                                return readpodulew((addr&0xC000)>>14,0,addr&0x3FFF);
                        case 7:
                                /* Podule space 4, 5, 6, 7 */
                                return readpodulew(((addr&0xC000)>>14)+4,0,addr&0x3FFF);
                        }
                }
                return 0xFFFFFFFF;
                break;

            case 0x08000000:
            case 0x09000000:
            case 0x0A000000:
            case 0x0B000000:
            case 0x0C000000:
            case 0x0D000000:
            case 0x0E000000:
            case 0x0F000000:
                /* EASI space */
//                        rpclog("EASI readl %08X\n",addr);
                return readpodulel((addr>>24)&7,1,addr&0xFFFFFF);

            case 0x10000000: /*SIMM 0 bank 0*/
            case 0x11000000:
            case 0x12000000:
            case 0x13000000:
                return ram[(addr & config.rammask) >> 2];

            case 0x14000000: /*SIMM 0 bank 1*/
            case 0x15000000:
            case 0x16000000:
            case 0x17000000:
                return ram2[(addr & config.rammask) >> 2];
            case 0x18000000: /*SIMM 1 bank 0*/
            case 0x1C000000: /*SIMM 1 bank 1*/
                return 0;
        }
        return 0;
/*        error("Bad readmeml %08X %08X at %07X\n",addr,addr&0x1F000000,PC);
        dumpregs();
        exit(-1);*/
}


uint32_t readmemfb(uint32_t addr)
{
        uint32_t addr2;

        if (mmu)
        {
                addr2=addr;
                if ((addr>>12)==readmemcache) addr=(addr&0xFFF)+readmemcache2;
                else
                {
                        readmemcache=addr>>12;
                        armirq&=~0x40;
                        addr=translateaddress(addr,0,0);
                        if (armirq&0x40)
                        {
                                readmemcache=0xFFFFFFFF;
                                return 0;
                        }
                        readmemcache2=addr&0xFFFFF000;
                }
                        switch (readmemcache2&0x1F000000)
                        {
                                case 0x00000000: /*ROM*/
                                vradd(addr2,&rom[((readmemcache2&0x7FF000)-(long)(addr2&~0xFFF))>>2],2,readmemcache2);
#ifdef _RPCEMU_BIG_ENDIAN
								addr2^=3;
#endif
                                return *(unsigned char *)((vraddrl[addr2>>12]&~3)+addr2);

                                case 0x02000000: /*VRAM*/
                                dirtybuffer[(addr & config.vrammask) >> 12] = 2;
                                vradd(addr2,&vram[((readmemcache2&0x7FF000)-(long)(addr2&~0xFFF))>>2],0,readmemcache2);
#ifdef _RPCEMU_BIG_ENDIAN
								addr2^=3;
#endif
                                return *(unsigned char *)(vraddrl[addr2>>12]+addr2);

                                case 0x10000000: /*SIMM 0 bank 0*/
                                case 0x11000000:
                                case 0x12000000:
                                case 0x13000000:
                                vradd(addr2, &ram[((readmemcache2 & config.rammask) - (long)(addr2 & ~0xFFF))>>2], 0, readmemcache2);
#ifdef _RPCEMU_BIG_ENDIAN
								addr2^=3;
#endif
                                return *(unsigned char *)(vraddrl[addr2>>12]+addr2);

                                case 0x14000000: /*SIMM 0 bank 1*/
                                case 0x15000000:
                                case 0x16000000:
                                case 0x17000000:
                                vradd(addr2, &ram2[((readmemcache2 & config.rammask) - (long)(addr2 & ~0xFFF))>>2], 0, readmemcache2);
#ifdef _RPCEMU_BIG_ENDIAN
								addr2^=3;
#endif
                                return *(unsigned char *)(vraddrl[addr2>>12]+addr2);
                        }
//                }
        }
        switch (addr&0x1F000000)
        {
                case 0x00000000: /*ROM*/
#ifdef _RPCEMU_BIG_ENDIAN
				addr^=3;
#endif
                return romb[addr&0x7FFFFF];
                case 0x02000000: /*VRAM*/
                if (!config.vrammask || config.model == CPUModel_ARM7500)
                  return 0xFF;
#ifdef _RPCEMU_BIG_ENDIAN
				addr^=3;
#endif
                return vramb[addr & config.vrammask];
                case 0x03000000: /*IO*/
                if (!(addr&0xC00000))
                {
                        /* 03000000 - 033fffff */
                        uint32_t bank = (addr >> 16) & 7;

                        switch (bank)
                        {
                        case 0:
                                /* IOMD Registers */
                                return readiomd(addr);
                        case 1:
                        case 2:
                                if (addr==0x03310000)
                                   return mouse_buttons_read();
                                if (addr>=0x03012000 && addr<=0x0302A000)
                                   return readfdcdma(addr);
                                if ((addr&0x00FFF400)==0x0002B000) /* Network podule */
                                   return 0xFFFFFFFF;
                                if (addr >= 0x3010000 && addr < 0x3012000) {
                                        /* SuperIO */
                                        return superio_read(addr);
                                }
                                break;
                        case 4:
                                /* Podule space 0, 1, 2, 3 */
                                return readpoduleb((addr&0xC000)>>14,0,addr&0x3FFF);
                        case 7:
                                /* Podule space 4, 5, 6, 7 */
                                return readpoduleb(((addr&0xC000)>>14)+4,0,addr&0x3FFF);
                        }
                }
                return 0xFFFFFFFF;
                break;
                case 0x08000000:
                case 0x09000000:
                case 0x0A000000:
                case 0x0B000000:
                case 0x0C000000:
                case 0x0D000000:
                case 0x0E000000:
                case 0x0F000000:
                    /* EASI space */
//                        rpclog("EASI readb %08X\n",addr);
                    return readpoduleb((addr>>24)&7,1,addr&0xFFFFFF);

                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
#ifdef _RPCEMU_BIG_ENDIAN
				addr^=3;
#endif
                return ramb[addr & config.rammask];
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
#ifdef _RPCEMU_BIG_ENDIAN
				addr^=3;
#endif
                return ramb2[addr & config.rammask];
        }
        return 0;
/*        error("Bad readmemb %08X at %07X\n",addr,PC);
        dumpregs();
        exit(-1);*/
}

#define HASH(l) (((l)>>2)&0x7FFF)

void writememfl(uint32_t addr, uint32_t val)
{
        uint32_t addr2=addr;

        if (mmu)
        {
                if ((addr>>12)==writememcache) addr=(addr&0xFFF)+writememcache2;
                else
                {
                        writememcache=addr>>12;
                        armirq&=~0x40;
                        addr=translateaddress(addr,1,0);
                        if (armirq&0x40)
                        {
                                writememcache=0xFFFFFFFF;
                                return;
                        }
                        writememcache2=addr&0xFFFFF000;
                }
                        switch (writememcache2&0x1F000000)
                        {
                                case 0x02000000: /*VRAM*/
                                dirtybuffer[(addr & config.vrammask) >> 12] = 2;
                                vwadd(addr2,&vram[((writememcache2&0x7FF000)-(long)(addr2&~0xFFF))>>2],0,writememcache2);
                                break;
                                case 0x10000000: /*SIMM 0 bank 0*/
                                if ((!config.vrammask || config.model == CPUModel_ARM7500) && (addr & 0x1FF00000) == 0x10000000)
                                   dirtybuffer[(addr & config.rammask) >> 12] = 2;
                                case 0x11000000:
                                case 0x12000000:
                                case 0x13000000:
                                vwadd(addr2, &ram[((writememcache2 & config.rammask) - (long)(addr2 & ~0xFFF))>>2], 0, writememcache2);
                                break;
                                case 0x14000000: /*SIMM 0 bank 1*/
                                case 0x15000000:
                                case 0x16000000:
                                case 0x17000000:
                                vwadd(addr2, &ram2[((writememcache2 & config.rammask) - (long)(addr2 & ~0xFFF))>>2], 0, writememcache2);
                                break;
                        }
        }

        switch (addr&0x1F000000)
        {
                case 0x02000000: /*VRAM*/
//                if (!config.vrammask) return;
                vram[(addr & config.vrammask) >> 2] = val;
                return;

                case 0x03000000: /*IO*/
                if (!(addr&0xC00000))
                {
                        uint32_t bank = (addr >> 16) & 7;

                        switch (bank)
                        {
                                case 0:
                                writeiomd(addr,val);
                                return;
                                case 1: case 2:
                                if (addr >= 0x3010000 && addr < 0x3012000) {
                                        /* SuperIO */
                                        if ((addr&0xFFC)==0x7C0)
                                        {
                                                ideboard=0;
                                                writeidew(val);
                                                return;
                                        }
                                        superio_write(addr, val);
                                        return;
                                }
                                if ((addr&0xFFF0000)==0x33A0000)
                                {
                                        return; /*Econet?*/
                                }
                                break;
                                case 4:
                                writepodulew((addr&0xC000)>>14,0,addr&0x3FFF,val>>16);
                                break;
                                case 7:
                                writepodulew(((addr&0xC000)>>14)+4,0,addr&0x3FFF,val>>16);
                                break;
                        }
                }
                if ((addr&0xC00000)==0x400000) /*VIDC20*/
                {
                        writevidc20(val);
                        return;
                }
                break;

                case 0x08000000:
                case 0x09000000:
                case 0x0A000000:
                case 0x0B000000:
                case 0x0C000000:
                case 0x0D000000:
                case 0x0E000000:
                case 0x0F000000:
                writepodulel((addr>>24)&7,1,addr&0xFFFFFF,val);
                return;

                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
                ram[(addr & config.rammask) >> 2] = val;
//                if (config.model == CPUModel_ARM7500 || (config.vrammask == 0 && (addr & config.rammask) < 0x100000))
//                   dirtybuffer[(addr & config.rammask) >> 12] = 1;
//                dirtybuffer[(addr & config.rammask) >> 10] = 1;
                return;

                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
                ram2[(addr & config.rammask) >> 2] = val;
//                dirtybuffer[(addr & config.rammask) >> 10] = 1;
                return;
                case 0x18000000: /*SIMM 1 bank 0*/
                case 0x1C000000: /*SIMM 1 bank 1*/
                return;
        }
/*        error("Bad writememl %08X %08X at %07X\n",addr,val,PC);
        dumpregs();
        exit(-1);*/
}

void writememfb(uint32_t addr, uint8_t val)
{
        uint32_t addr2=addr;

        if (mmu)
        {
                if ((addr>>12)==writemembcache) addr=(addr&0xFFF)+writemembcache2;
                else
                {
                        writemembcache=addr>>12;
                        armirq&=~0x40;
                        addr=translateaddress(addr,1,0);
                        if (armirq&0x40)
                        {
                                writemembcache=0xFFFFFFFF;
                                return;
                        }
                        writemembcache2=addr&0xFFFFF000;
                }
//                        if ((addr&0x1F000000)==0x2000000)
//                           dirtybuffer[(addr & config.vrammask) >> 12] = 2;
                        switch (writemembcache2&0x1F000000)
                        {
                                case 0x02000000: /*VRAM*/
                                dirtybuffer[(addr & config.vrammask) >> 12] = 2;
                                vwadd(addr2,&vram[((writemembcache2&0x7FF000)-(long)(addr2&~0xFFF))>>2],0,writemembcache2);
                                break;
                                case 0x10000000: /*SIMM 0 bank 0*/
                                if ((!config.vrammask || config.model == CPUModel_ARM7500) && (addr & 0x1FF00000) == 0x10000000)
                                   dirtybuffer[(addr & config.rammask) >> 12] = 2;
                                case 0x11000000:
                                case 0x12000000:
                                case 0x13000000:
                                vwadd(addr2, &ram[((writemembcache2 & config.rammask) - (long)(addr2 & ~0xFFF)) >> 2], 0, writemembcache2);
                                break;
                                case 0x14000000: /*SIMM 0 bank 1*/
                                case 0x15000000:
                                case 0x16000000:
                                case 0x17000000:
                                vwadd(addr2, &ram2[((writemembcache2 & config.rammask) - (long)(addr2 & ~0xFFF)) >> 2], 0, writemembcache2);
                                break;
                        }
        }

        switch (addr&0x1F000000)
        {
                case 0x02000000: /*VRAM*/
//                if (!config.vrammask) return;
#ifdef _RPCEMU_BIG_ENDIAN
				addr^=3;
#endif
                vramb[addr & config.vrammask] = val;
                return;
                case 0x03000000: /*IO*/
                if (!(addr&0xC00000))
                {
                        uint32_t bank = (addr >> 16) & 7;

                        switch (bank)
                        {
                                case 0:
                                writeiomd(addr,val);
                                return;
                                case 1: case 2:
                                if (addr==0x3310000) return;
                                if ((addr&0xFC0000)==0x240000) return;
                                if (addr>=0x3012000 && addr<=0x302A000)
                                {
                                        writefdcdma(addr,val);
                                        return;
                                }
                                if (addr >= 0x3010000 && addr < 0x3012000) {
                                        /* SuperIO */
                                        superio_write(addr, val);
                                        return;
                                }
                                if ((addr&0xFFF0000)==0x33A0000)
                                {
                                        return; /*Econet?*/
                                }
                                break;
                                case 4:
                                writepoduleb((addr&0xC000)>>14,0,addr&0x3FFF,val);
                                break;
                                case 7:
                                writepoduleb(((addr&0xC000)>>14)+4,0,addr&0x3FFF,val);
                                break;
                        }
                }
                break;
                
                case 0x08000000:
                case 0x09000000:
                case 0x0A000000:
                case 0x0B000000:
                case 0x0C000000:
                case 0x0D000000:
                case 0x0E000000:
                case 0x0F000000:
                writepoduleb((addr>>24)&7,1,addr&0xFFFFFF,val);
                return;

                case 0x10000000: /*SIMM 0 bank 0*/
                case 0x11000000:
                case 0x12000000:
                case 0x13000000:
#ifdef _RPCEMU_BIG_ENDIAN
				addr^=3;
#endif
                ramb[addr & config.rammask] = val;
//                if (config.model == CPUModel_ARM7500 || (config.vrammask == 0 && (addr & config.rammask) < 0x100000))
//                   dirtybuffer[(addr & config.rammask) >> 12] = 1;
                return;
                case 0x14000000: /*SIMM 0 bank 1*/
                case 0x15000000:
                case 0x16000000:
                case 0x17000000:
#ifdef _RPCEMU_BIG_ENDIAN
				addr^=3;
#endif
                ramb2[addr & config.rammask] = val;
//                dirtybuffer[(addr & config.rammask) >> 10] = 1;
                return;
        }
/*        error("Bad writememb %08X %02X at %07X\n",addr,val,PC);
        dumpregs();
        exit(-1);*/
}
