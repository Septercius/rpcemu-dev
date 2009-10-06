/*RPCemu v0.6 by Tom Walker
  ARM6/7 emulation*/

#include "rpcemu.h"

#ifndef DYNAREC

#if defined WIN32 || defined _WIN32 || defined _WIN32
#include <windows.h>
#endif

int swiout=0;
int times8000=0;
/*3/12/06 - databort and prefabort have been rolled into bits 6 and 7 of armirq.
  This gives a minor speedup.
  MSR fixes from John-Mark Bell allow RISC OS 6 to work.

  31/10/06 - Various optimisations, mainly to ldmstm()
  I altered the most frequently used LDM/STM instructions to streamline the inner
  loops, which gives a 20-25% speed boost for those instructions. The most frequently
  used by far is 0x92 (STMDB !, used 5x more than any other) followed by 0x89, 0x8B,
  0x93 and 0x95.
  Also various other optimisations, eg shifts in non-S instructions are mostly inlined,
  except for those which require 'special attention', where shift amounts are outside
  the usual boundaries. Will do the same for S instructions.
  Other optimisations to memory system, !FreeDoom has gone from 24 MIPS to 31 MIPS,
  OpenTTD much the same.Desktop seems to have less gains, though Dhrystone has gone from
  40.4 DMIPS to 46 DMIPS*/
  
/*There are bits and pieces of StrongARM emulation in this file. All of the
  extra instructions have been identified, but only some of the long multiplication
  instructions have been defined (enough to get AMPlayer working in SA mode).
  Due to this, it has been defined out for now. Uncomment the following line to
  emulate what is there for now
  ArcQuake appears totally broken with this turned on, so there are obviously some
  bugs in the new instructions

  30/10/06 - Long multiplication instructions fixed, feel free to leave this in now!*/
#define STRONGARM

/*Preliminary FPA emulation. This works to an extent - !Draw works with it, !SICK
  seems to (FPA Whetstone scores are around 100x without), but !AMPlayer doesn't
  work, and GCC stuff tends to crash.*/
//#define FPA

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static int inscounts[256];

#include "config.h"

#include "hostfs.h"
#include "arm.h"
#include "cp15.h"
#include "mem.h"
#include "iomd.h"
#include "keyboard.h"
#include "fdc.h"
#include "ide.h"

#ifdef RPCEMU_LINUX
#include "network-linux.h"
#endif

int blockend;
static int r15diff;
//static int r11check=0;
static int fdci=0;
//static int out2=0;
//static int r8match=0;
static int mmask;
uint32_t r15mask;
int memmode;
int irq;
static int cycles;
int prefabort;
uint32_t rotatelookup[4096];
uint32_t inscount;
int armirq=0;
int cpsr;
static uint32_t *pcpsr;

uint32_t *usrregs[16];
static uint32_t userregs[17], superregs[17], fiqregs[17], irqregs[17];
static uint32_t abortregs[17], undefregs[17];
static uint32_t spsr[16];
uint32_t armregs[18];
uint32_t mode;
int databort;
int prog32;


#define USER       0
#define FIQ        1
#define IRQ        2
#define SUPERVISOR 3
#define ABORT      7
#define UNDEFINED  11
#define SYSTEM     15

#define NFSET ((armregs[cpsr]&0x80000000)?1:0)
#define ZFSET ((armregs[cpsr]&0x40000000)?1:0)
#define CFSET ((armregs[cpsr]&0x20000000)?1:0)
#define VFSET ((armregs[cpsr]&0x10000000)?1:0)

#define NFLAG 0x80000000
#define ZFLAG 0x40000000
#define CFLAG 0x20000000
#define VFLAG 0x10000000
#define IFLAG 0x08000000

#define GETADDR(r) ((r==15)?(armregs[15]&r15mask):armregs[r])
#define LOADREG(r,v) if (r==15) { armregs[15]=(armregs[15]&~r15mask)|(((v)+4)&r15mask); refillpipeline(); } else armregs[r]=(v);
#define GETREG(r) ((r==15) ? armregs[15]+4 : armregs[r])
#define LDRRESULT(a,v) ((a&3)?(v>>((a&3)<<3))|(v<<(((a&3)^3)<<3)):v)

#define refillpipeline()

#include "arm_common.h"

uint32_t ins=0;

uint32_t pccache,*pccache2;

void updatemode(uint32_t m)
{
        uint32_t c,om=mode;

        usrregs[15]=&armregs[15];
        switch (mode&15) /*Store back registers*/
        {
            case USER:
            case SYSTEM: /* System (ARMv4) shares same bank as User mode */
                for (c=8;c<15;c++) userregs[c]=armregs[c];
                break;

            case IRQ:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                irqregs[0]=armregs[13];
                irqregs[1]=armregs[14];
                break;

            case FIQ:
                for (c=8;c<15;c++) fiqregs[c]=armregs[c];
                break;

            case SUPERVISOR:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                superregs[0]=armregs[13];
                superregs[1]=armregs[14];
                break;

            case ABORT:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                abortregs[0]=armregs[13];
                abortregs[1]=armregs[14];
                break;

            case UNDEFINED:
                for (c=8;c<13;c++) userregs[c]=armregs[c];
                undefregs[0]=armregs[13];
                undefregs[1]=armregs[14];
                break;
        }
        mode=m;

        switch (m&15)
        {
            case USER:
            case SYSTEM:
                for (c=8;c<15;c++) armregs[c]=userregs[c];
                for (c=0;c<15;c++) usrregs[c]=&armregs[c];
                break;

            case IRQ:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=irqregs[0];
                armregs[14]=irqregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                break;
            
            case FIQ:
                for (c=8;c<15;c++) armregs[c]=fiqregs[c];
                for (c=0;c<8;c++)  usrregs[c]=&armregs[c];
                for (c=8;c<15;c++) usrregs[c]=&userregs[c];
                break;

            case SUPERVISOR:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=superregs[0];
                armregs[14]=superregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                break;
            
            case ABORT:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=abortregs[0];
                armregs[14]=abortregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                break;

            case UNDEFINED:
                for (c=8;c<13;c++) armregs[c]=userregs[c];
                armregs[13]=undefregs[0];
                armregs[14]=undefregs[1];
                for (c=0;c<13;c++) usrregs[c]=&armregs[c];
                for (c=13;c<15;c++) usrregs[c]=&userregs[c];
                break;

            default:
                error("Bad mode %i\n",mode);
                dumpregs();
                exit(-1);
        }

        if (mode&16)
        {
                mmask=31;
                cpsr=16;
                pcpsr=&armregs[16];
//                printf("Now 32-bit mode %i %08X %i\n",mode&15,PC,ins);
                r15mask=0xFFFFFFFC;
                if (!(om&16))
                {
                        armregs[16]=(armregs[15]&0xF0000000)|mode;
                        armregs[16]|=((armregs[15]&0xC000000)>>20);
                        armregs[15]&=0x3FFFFFC;
  //                      if (output) printf("Switching to 32-bit mode : CPSR %08X\n",armregs[16]);
                }
        }
        else
        {
                mmask=3;
                cpsr=15;
                pcpsr=&armregs[15];
//                printf("Now 26-bit mode %i %08X %i\n",mode&15,PC,ins);
                r15mask=0x3FFFFFC;
                armregs[16]=(armregs[16]&0xFFFFFFE0)|mode;
                if (om&16)
                {
                        armregs[15]&=r15mask;
                        armregs[15]|=(mode&3);
                        armregs[15]|=(armregs[16]&0xF0000000);
                        armregs[15]|=((armregs[16]&0xC0)<<20);
                }
        }

	/* Update memory access mode based on privilege level of ARM mode */
	memmode = ARM_MODE_PRIV(mode) ? 1 : 0;
}

static int stmlookup[256];

int countbitstable[65536];

void resetarm(void)
{
        int c,d,exec = 0,data;
//        atexit(dumpregs);
        uint32_t rotval,rotamount;
        for (c=0;c<256;c++) inscounts[c]=0;
        for (c=0;c<256;c++)
        {
                stmlookup[c]=0;
                for (d=0;d<8;d++)
                {
                        if (c&(1<<d)) stmlookup[c]+=4;
                }
        }
        for (c=0;c<65536;c++)
        {
                countbitstable[c]=0;
                for (d=0;d<16;d++)
                {
                        if (c&(1<<d)) countbitstable[c]+=4;
                }
        }
        r15mask=0x3FFFFFC;
        pccache=0xFFFFFFFF;
        updatemode(SUPERVISOR);
        cpsr=15;
//        prog32=1;
        for (c=0;c<16;c++)
        {
                for (d=0;d<16;d++)
                {
                        armregs[15]=d<<28;
                        switch (c)
                        {
                                case 0:  /*EQ*/ exec=ZFSET; break;
                                case 1:  /*NE*/ exec=!ZFSET; break;
                                case 2:  /*CS*/ exec=CFSET; break;
                                case 3:  /*CC*/ exec=!CFSET; break;
                                case 4:  /*MI*/ exec=NFSET; break;
                                case 5:  /*PL*/ exec=!NFSET; break;
                                case 6:  /*VS*/ exec=VFSET; break;
                                case 7:  /*VC*/ exec=!VFSET; break;
                                case 8:  /*HI*/ exec=(CFSET && !ZFSET); break;
                                case 9:  /*LS*/ exec=(!CFSET || ZFSET); break;
                                case 10: /*GE*/ exec=(NFSET == VFSET); break;
                                case 11: /*LT*/ exec=(NFSET != VFSET); break;
                                case 12: /*GT*/ exec=(!ZFSET && (NFSET==VFSET)); break;
                                case 13: /*LE*/ exec=(ZFSET || (NFSET!=VFSET)); break;
                                case 14: /*AL*/ exec=1; break;
                                case 15: /*NV*/ exec=0; break;
                        }
                        flaglookup[c][d]=exec;
                }
        }

        for (data=0;data<4096;data++)
        {
                rotval=data&0xFF;
                rotamount=((data>>8)&0xF)<<1;
                rotval=(rotval>>rotamount)|(rotval<<(32-rotamount));
                rotatelookup[data]=rotval;
        }

        armregs[15]=0x0C000008|3;
        armregs[16]=SUPERVISOR|0xD0;
        mode=SUPERVISOR;
        resetcp15();
        pccache=0xFFFFFFFF;
        if (config.model == CPUModel_SA110 || config.model == CPUModel_ARM810)
                r15diff = 0;
        else
                r15diff = 4;
}

int indumpregs=0;
int insnum[256];

void dumpregs(void)
{
        char s[1024];

        if (indumpregs) return;
        indumpregs=1;

        sprintf(s, "R 0=%08X R 4=%08X R 8=%08X R12=%08X\n"
                   "R 1=%08X R 5=%08X R 9=%08X R13=%08X\n"
                   "R 2=%08X R 6=%08X R10=%08X R14=%08X\n"
                   "R 3=%08X R 7=%08X R11=%08X R15=%08X\n"
                   "%u %s\n"
                   "%08X %08X %08X",
                   armregs[0], armregs[4], armregs[8], armregs[12],
                   armregs[1], armregs[5], armregs[9], armregs[13],
                   armregs[2], armregs[6], armregs[10], armregs[14],
                   armregs[3], armregs[7], armregs[11], armregs[15],
                   ins, (mmu) ? "MMU enabled" : "MMU disabled",
                   oldpc, oldpc2, oldpc3);
        rpclog("%s",s);
        printf("%s",s);

        memmode=1;

        indumpregs=0;
}

#define dumpregs()

static uint32_t shift3(uint32_t opcode)
{
        uint32_t shiftmode=opcode&0x60;//(opcode>>5)&3;
        uint32_t shiftamount=(opcode>>7)&31;
        uint32_t temp;
        int cflag=CFSET;
        if (opcode&0x10)
        {
                shiftamount=armregs[(opcode>>8)&15]&0xFF;
                if (shiftmode==3)
                   shiftamount&=0x1F;
        }
        temp=armregs[RM];
        if (shiftamount) armregs[cpsr]&=~CFLAG;
        switch (shiftmode)
        {
                case 0: /*LSL*/
                if (!shiftamount) return temp;
                if (shiftamount==32)
                {
                        if (temp&1) armregs[cpsr]|=CFLAG;
                        return 0;
                }
                if (shiftamount>32) return 0;
                if ((temp<<(shiftamount-1))&0x80000000) armregs[cpsr]|=CFLAG;
                return temp<<shiftamount;

                case 0x20: /*LSR*/
                if (!shiftamount && !(opcode&0x10))
                {
                        shiftamount=32;
                }
                if (!shiftamount) return temp;
                if (shiftamount==32)
                {
                        if (temp&0x80000000) armregs[cpsr]|=CFLAG;
                        else                 armregs[cpsr]&=~CFLAG;
                        return 0;
                }
                if (shiftamount>32) return 0;
                if ((temp>>(shiftamount-1))&1) armregs[cpsr]|=CFLAG;
                return temp>>shiftamount;

                case 0x40: /*ASR*/
                if (!shiftamount)
                {
                        if (opcode&0x10) return temp;
                }
                if (shiftamount>=32 || !shiftamount)
                {
                        if (temp&0x80000000) armregs[cpsr]|=CFLAG;
                        else                 armregs[cpsr]&=~CFLAG;
                        if (temp&0x80000000) return 0xFFFFFFFF;
                        return 0;
                }
                if (((int)temp>>(shiftamount-1))&1) armregs[cpsr]|=CFLAG;
                return (int)temp>>shiftamount;

                default: /*ROR*/
                armregs[cpsr]&=~CFLAG;
                if (!shiftamount && !(opcode&0x10))
                {
                        if (temp&1) armregs[cpsr]|=CFLAG;
                        return (((cflag)?1:0)<<31)|(temp>>1);
                }
                if (!shiftamount)
                {
                        armregs[cpsr]|=cflag;
                        return temp;
                }
                if (!(shiftamount&0x1F))
                {
                        if (temp&0x80000000) armregs[cpsr]|=CFLAG;
                        return temp;
                }
                if (((temp>>shiftamount)|(temp<<(32-shiftamount)))&0x80000000) armregs[cpsr]|=CFLAG;
                return (temp>>shiftamount)|(temp<<(32-shiftamount));
                break;
        }
}

#define shift(o)  ((o&0xFF0)?shift3(o):armregs[RM])
#define shift2(o) ((o&0xFF0)?shift4(o):armregs[RM])
#define shift_ldrstr(o) shift2(o)
//#define shift_ldrstr(o) ((o&0xFF0)?shift_ldrstr2(o):armregs[RM])

//#if 0
static unsigned
shift5(unsigned opcode, unsigned shiftmode, unsigned shiftamount, uint32_t rm)
{
                switch (shiftmode)
                {
                        case 0: /*LSL*/
                        if (!shiftamount)    return rm;
                        return 0; /*shiftamount>=32*/

                        case 0x20: /*LSR*/
                        if (!shiftamount && (opcode&0x10)) return rm;
                        return 0; /*shiftamount>=32*/

                        case 0x40: /*ASR*/
                        if (!shiftamount && !(opcode&0x10)) shiftamount=32;
                        if (shiftamount>=32)
                        {
                                if (rm&0x80000000)
                                   return 0xFFFFFFFF;
                                return 0;
                        }
                        return (int)rm>>shiftamount;

                        case 0x60: /*ROR*/
                        if (!(opcode&0x10)) return (((CFSET)?1:0)<<31)|(rm>>1);
                        shiftamount&=0x1F;
                        return (rm>>shiftamount)|(rm<<(32-shiftamount));

                        default:
                        error("Shift2 mode %u amount %u\n", shiftmode, shiftamount);
                        dumpregs();
                        exit(-1);
                }
}
//#endif

static inline unsigned shift4(unsigned opcode)
{
        unsigned shiftmode=opcode&0x60;
        unsigned shiftamount=(opcode&0x10)?(armregs[(opcode>>8)&15]&0xFF):((opcode>>7)&31);
        uint32_t rm=armregs[RM];
        if ((shiftamount-1)>=31)
        {
                return shift5(opcode,shiftmode,shiftamount,rm);
        }
        else
        {
                switch (shiftmode)
                {
                        case 0: /*LSL*/
                        return rm<<shiftamount;
                        case 0x20: /*LSR*/
                        return rm>>shiftamount;
                        case 0x40: /*ASR*/
                        return (int)rm>>shiftamount;
                        default: /*ROR*/
                        return (rm>>shiftamount)|(rm<<(32-shiftamount));
                }
        }
}
#if 0
unsigned shift_ldrstr3(unsigned opcode, unsigned shiftmode, unsigned shiftamount, uint32_t rm)
{
                switch (shiftmode)
                {
                        case 0: /*LSL*/
                        return rm;
                        case 0x20: /*LSR*/
                        return 0;
                        case 0x40: /*ASR*/
                        if (rm&0x80000000) return 0xFFFFFFFF;
                        return 0;
                        default: /*ROR*/
                        return (((*pcpsr)/*armregs[cpsr]*/&CFLAG)<<2)|(rm>>1);
//                        return (((CFSET)?1:0)<<31)|(rm>>1);
                }
}

static inline unsigned shift_ldrstr2(unsigned opcode)
{
        unsigned shiftmode=opcode&0x60;
        unsigned shiftamount=(opcode>>7)&31;
        uint32_t rm=armregs[RM];
        if (!shiftamount)
        {
                return 0;
//                shift_ldrstr3(opcode,shiftmode,shiftamount,rm);
        }
        else
        {
                switch (shiftmode)
                {
                        case 0: /*LSL*/
                        return rm<<shiftamount;
                        case 0x20: /*LSR*/
                        return rm>>shiftamount;
                        case 0x40: /*ASR*/
                        return (int)rm>>shiftamount;
                        default: /*ROR*/
                        return (rm>>shiftamount)|(rm<<(32-shiftamount));
                }
        }
}
#endif
static inline unsigned rotate(unsigned data)
{
        uint32_t rotval;
        rotval=rotatelookup[data&4095];
        if (/*data&0x100000 && */data&0xF00)
        {
                if (rotval&0x80000000) armregs[cpsr]|=CFLAG;
                else                   armregs[cpsr]&=~CFLAG;
        }
        return rotval;
}

static const int ldrlookup[4]={0,8,16,24};

#define ldrresult(v,a) ((v>>ldrlookup[addr&3])|(v<<(32-ldrlookup[addr&3])))

#define undefined() exception(UNDEFINED,8,4)

static void bad_opcode(uint32_t opcode) 
{
     error("Bad opcode %02X %08X at %07X\n",(opcode >> 20) & 0xFF, opcode, PC);
     rpclog("Bad opcode %02X %08X at %07X\n",(opcode >> 20) & 0xFF, opcode, PC);
     dumpregs();
     exit(EXIT_FAILURE);
}

void exception(int mmode, uint32_t address, int diff)
{
        uint32_t templ;
        unsigned char irq_disable;

	/* If FIQ exception, disable FIQ and IRQ, otherwise disable just IRQ */
	if (mmode == FIQ) {
		irq_disable = (0x80 | 0x40);
	} else {
		irq_disable = 0x80;
	}

        if (mode&16)
        {
                templ=armregs[15]-diff;
                spsr[mmode]=armregs[16];
                updatemode(mmode|16);
                armregs[14]=templ;
                armregs[16]&=~0x1F;
                armregs[16] |= 0x10 | mmode | irq_disable;
                armregs[15]=address;
                refillpipeline();
        }
        else if (prog32)
        {
                templ=armregs[15]-diff;
                updatemode(mmode|16);
                armregs[14]=templ&0x3FFFFFC;
                spsr[mmode]=(armregs[16]&~0x1F)|(templ&3);
                spsr[mmode]&=~0x10;
                armregs[16] |= irq_disable;
                armregs[15]=address;
                refillpipeline();
        }
        else
        {
                templ=armregs[15]-diff;
                armregs[15]|=3;
                /* When in 26-bit config, Abort and Undefined exceptions enter
                   mode SVC_26 */
                updatemode(mmode >= SUPERVISOR ? SUPERVISOR : mmode);
                armregs[14]=templ;
                armregs[15]&=0xFC000003;
                armregs[15] |= ((irq_disable << 20) | address);
                refillpipeline();
        }
}
#if 0
#define writememfast(a,v) writememl(a,v)

static inline void ldmstm(uint32_t ls_opcode, uint32_t opcode)
{
  uint32_t templ, mask, addr, c;
  uint32_t *rn;
//                                inscounts[(opcode>>20)&0xFF]++;
  //  addr = mem_getphys(armregs[RN]);
  addr = armregs[RN];
  switch (ls_opcode) 
    {

    case 0x80: /*STMDA*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    if (c==15) { writememl(addr,armregs[c]+r15diff); }
                    else       { writememl(addr,armregs[c]); }
                    addr-=4;
//                    cycles--;
            }
            mask>>=1;
    }
//    cycles-=2;
    break;

    case 0x82: /*STMDA !*/
        rn=&armregs[RN];
        templ=stmlookup[opcode&0xFF]+stmlookup[(opcode>>8)&0xFF];
        c=15;
        while (!(opcode&0x8000))
        {
                opcode<<=1;
                c--;
        }
        if (c==15) { writememfast(addr,armregs[15]+r15diff); }
        else       { writememfast(addr,armregs[c]); }
        addr-=4;
        c--;
        *rn-=templ;
        for (c;c<16;c--)
        {
                if (opcode&0x4000)
                {
                        writememfast(addr,armregs[c]);
                        addr-=4;
                }
                opcode<<=1;
        }
    break;
                                
    case 0x83: /*LDMDA !*/
    if (!(opcode&(1<<RN)))
    {
            mask=0x8000;
            for (c=15;c<16;c--)
            {
                    if (opcode&mask)
                    {
                            if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                            else       armregs[c]=readmeml(addr);
                            addr-=4;
                            armregs[RN]-=4;
//                            cycles--;
                    }
                    mask>>=1;
            }
            if (opcode&0x8000) refillpipeline();
//            cycles-=3;
            return;
    }
    case 0x81: /*LDMDA*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                    else       armregs[c]=readmeml(addr);
                    addr-=4;
//                    //cycles--;
            }
            mask>>=1;
    }
    if (opcode&0x8000) refillpipeline();
//    //cycles-=3;
    break;


    case 0x85: /*LDMDA ^*/
    mask=0x8000;
    if (opcode&0x8000)
    {
            for (c=15;c<16;c--)
            {
                    if (opcode&mask)
                    {
                            if (c==15 && !(armregs[15]&3) && !(mode&16))
                               armregs[15]=(readmeml(addr)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                            else
                               armregs[c]=readmeml(addr);
                            if (c==15 && (mode&16)) armregs[cpsr]=spsr[mode&15];
                            addr-=4;
//                            //cycles--;
                    }
                    mask>>=1;
            }
            armregs[15]+=4;
            if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
            refillpipeline();
    }
    else
    {
            for (c=15;c<16;c--)
            {
                    if (opcode&mask)
                    {
                            *usrregs[c]=readmeml(addr);
                            addr-=4;
  //                          //cycles--;
                    }
                    mask>>=1;
            }
    }
//    //cycles-=3;
    break;

    case 0x88: /*STMIA*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    if (c==15) { writememl(addr,armregs[c]+r15diff); }
                    else       { writememl(addr,armregs[c]); }
                    addr+=4;
//                    //cycles--;
            }
            mask<<=1;
    }
//    //cycles-=2;
    break;


    case 0x8A: /*STMIA !*/
        rn=&armregs[RN];
        templ=stmlookup[opcode&0xFF]+stmlookup[(opcode>>8)&0xFF];
        c=0;
        while (!(opcode&1))
        {
                opcode>>=1;
                c++;
        }
        if (c==15) { writememfast(addr,armregs[15]+r15diff); }
        else       { writememfast(addr,armregs[c]); }
        addr+=4;
        c++;
        *rn+=templ;
        for (;c<16;c++)
        {
                if (opcode&2)
                {
                        if (c==15) { writememfast(addr,armregs[15]+r15diff); }
                        else       { writememfast(addr,armregs[c]); }
                        addr+=4;
                }
                opcode>>=1;
        }
        break;

        case 0x8B: /*LDMIA !*/
        if (!(opcode&(1<<RN)))
        {
                templ=RN;
                for (c=0;c<15;c++)
                {
                        if (opcode&1)
                        {
                                armregs[c]=readmeml(addr);
                                addr+=4;
                        }
                        opcode>>=1;
                }
                if (opcode&1)
                {
                        armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                        addr+=4;
                        refillpipeline();
                }
                armregs[templ]=addr;
                return;
        }
        case 0x89: /*LDMIA*/
        for (c=0;c<15;c++)
        {
                if (opcode & 1)
                {
                        armregs[c]=readmeml(addr);
                        addr+=4;
                }
                opcode>>=1;
        }
        if (opcode&1)
        {
	       armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
	       refillpipeline();
        }
        break;
                                
    case 0x8C: /*STMIA ^*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    if (c==15) { writememl(addr,*usrregs[c]+r15diff); }
                    else       { writememl(addr,*usrregs[c]); }
                    addr+=4;
//                    //cycles--;
            }
            mask<<=1;
    }
//    //cycles-=2;
    break;

    case 0x8D: /*LDMIA ^*/
    mask=1;
    if (opcode&0x8000)
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            if (c==15 && !(armregs[15]&3) && !(mode&16))
                               armregs[15]=(readmeml(addr)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                            else
                               armregs[c]=readmeml(addr);
                            if (c==15 && (mode&16)) armregs[cpsr]=spsr[mode&15];
                            addr+=4;
//                            //cycles--;
                    }
                    mask<<=1;
            }
            armregs[15]+=4;
            if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
            refillpipeline();
    }
    else
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            *usrregs[c]=readmeml(addr);
                            addr+=4;
//                            //cycles--;
                    }
                    mask<<=1;
            }
    }
//    //cycles-=3;
    break;

    case 0x8F: /*LDMIA !^*/
    mask=1;
    if (opcode&0x8000)
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            if (c==15 && !(armregs[15]&3) && !(mode&16))
                               armregs[15]=(readmeml(addr)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                            else
                               armregs[c]=readmeml(addr);
                            if (c==15 && (mode&16)) armregs[cpsr]=spsr[mode&15];
                            addr+=4;
                            armregs[RN]+=4;
//                            //cycles--;
                    }
                    mask<<=1;
            }
            armregs[15]+=4;
            if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
            refillpipeline();
    }
    else
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            *usrregs[c]=readmeml(addr);
                            addr+=4;
                            armregs[RN]+=4;
//                            //cycles--;
                    }
                    mask<<=1;
            }
    }
//    //cycles-=3;
    break;

    case 0x90: /*STMDB*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    addr-=4;
                    if (c==15) { writememl(addr,armregs[c]+r15diff); }
                    else       { writememl(addr,armregs[c]); }
//                    //cycles--;
            }
            mask>>=1;
    }
//    //cycles-=2;
    break;

        case 0x92: /*STMDB !*/
        rn=&armregs[RN];
        templ=stmlookup[opcode&0xFF]+stmlookup[(opcode>>8)&0xFF];
        c=15;
        while (!(opcode&0x8000))
        {
                opcode<<=1;
                c--;
        }
        addr-=4;
        if (c==15) { writememfast(addr,armregs[15]+r15diff); }
        else       { writememfast(addr,armregs[c]); }
        c--;
        *rn-=templ;
        for (c;c<16;c--)
        {
                if (opcode&0x4000)
                {
                        addr-=4;
                        writememfast(addr,armregs[c]);
                }
                opcode<<=1;
        }
        break;

        case 0x93: /*LDMDB !*/
        if (!(opcode&(1<<RN)))
        {
                templ=RN;
                if (opcode&0x8000)
                {
                        addr-=4;
                        armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                        refillpipeline();
                }
                for (c=14;c<16;c--)
                {
                        if (opcode&0x4000)
                        {
                                addr-=4;
                                armregs[c]=readmeml(addr);
                        }
                        opcode<<=1;
                }
                armregs[templ]=addr;
                return;
        }
        case 0x91: /*LDMDB*/
        if (opcode&0x8000)
        {
                addr-=4;
                armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                refillpipeline();
        }
        for (c=14;c<16;c--)
        {
                if (opcode&0x4000)
                {
                        addr-=4;
                        armregs[c]=readmeml(addr);
                }
                opcode<<=1;
        }
        break;

    case 0x94: /*STMDB ^*/
    mask=0x8000;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    addr-=4;
                    if (c==15) { writememl(addr,*usrregs[c]+r15diff); }
                    else       { writememl(addr,*usrregs[c]); }
//                    //cycles--;
            }
            mask>>=1;
    }
//    //cycles-=2;
    break;

        case 0x95: /*LDMDB ^*/
        mask=0x8000;
        if (opcode&0x8000)
        {
                addr-=4;
                if (!mode) armregs[15]=(readmeml(addr)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                else       armregs[15]=readmeml(addr);
                if (mode&16)
                {
                        armregs[cpsr]=spsr[mode&15];
                        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                }
                for (c=14;c<16;c--)
                {
                        if (opcode&0x4000)
                        {
                                addr-=4;
                                armregs[c]=readmeml(addr);
                        }
                        opcode<<=1;
                }
                armregs[15]+=4;
                refillpipeline();
        }
        else
        {
                for (c=14;c<16;c--)
                {
                        if (opcode&0x4000)
                        {
                                addr-=4;
                                *usrregs[c]=readmeml(addr);
                        }
                        opcode<<=1;
                }
        }
        break;

    case 0x96: /*STMDB !^*/
    mask=0x8000;
    templ=0;
    for (c=15;c<16;c--)
    {
            if (opcode&mask)
            {
                    addr-=4;
                    armregs[RN]-=4;
                    if (c==15)                { writememl(addr,*usrregs[c]+r15diff); }
                    else if (c==RN && !templ) { writememl(addr,*usrregs[c]+4); }
                    else                      { writememl(addr,*usrregs[c]); }
//                    //cycles--;
            }
            mask>>=1;
    }
//    //cycles-=2;
    break;
    case 0x98: /*STMIB*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    addr+=4;
                    if (c==15) { writememl(addr,armregs[c]+r15diff); }
                    else       { writememl(addr,armregs[c]); }
//                    //cycles--;
            }
            mask<<=1;
    }
//    //cycles-=2;
    break;
 
    case 0x9A: /*STMIB !*/
        rn=&armregs[RN];
        templ=stmlookup[opcode&0xFF]+stmlookup[(opcode>>8)&0xFF];
        c=0;
        while (!(opcode&1))
        {
                opcode>>=1;
                c++;
        }
        addr+=4;
        if (c==15) { writememfast(addr,armregs[15]+r15diff); }
        else       { writememfast(addr,armregs[c]); }
        c++;
        *rn+=templ;
        for (;c<16;c++)
        {
                if (opcode&2)
                {
                        addr+=4;
                        if (c==15) { writememfast(addr,armregs[15]+r15diff); }
                        else       { writememfast(addr,armregs[c]); }
                }
                opcode>>=1;
        }
/*    mask=1;
    templ=0;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    addr+=4;
                    armregs[RN]+=4;
                    if (c==15)                { writememl(addr,armregs[c]+r15diff); }
                    else if (c==RN && !templ) { writememl(addr,armregs[c]-4); }
                    else                      { writememl(addr,armregs[c]); }
//                    //cycles--;
            }
            mask<<=1;
    }*/
//    //cycles-=2;
    break;

    case 0x9B: /*LDMIB !*/
    if (!(opcode&(1<<RN)))
    {
            mask=1;
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            addr+=4;
                            armregs[RN]+=4;
                            if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                            else       armregs[c]=readmeml(addr);
//                            //cycles--;
                    }
                    mask<<=1;
            }
            if (opcode&0x8000) refillpipeline();
//            //cycles-=3;
            return;
    }
    case 0x99: /*LDMIB*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    addr+=4;
                    if (c==15) armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask);
                    else       armregs[c]=readmeml(addr);
//                    //cycles--;
            }
            mask<<=1;
    }
    if (opcode&0x8000) refillpipeline();
//    //cycles-=3;
    break;
    case 0x9C: /*STMIB ^*/
    mask=1;
    for (c=0;c<16;c++)
    {
            if (opcode&mask)
            {
                    addr+=4;
                    if (c==15) { writememl(addr,*usrregs[c]+r15diff); }
                    else       { writememl(addr,*usrregs[c]); }
//                    //cycles--;
            }
            mask<<=1;
    }
//    //cycles-=2;
    break;

    case 0x9D: /*LDMIB ^*/
    mask=1;
    if (opcode&0x8000)
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            addr+=4;
                            if (c==15 && !(armregs[15]&3) && !(mode&16))
                               armregs[15]=(readmeml(addr)&0xF3FFFFFC)|(armregs[15]&0xC000003);
                            else
                               armregs[c]=readmeml(addr);
                            if (c==15 && (mode&16)) armregs[cpsr]=spsr[mode&15];
//                            //cycles--;
                    }
                    mask<<=1;
            }
            armregs[15]+=4;
            if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
            refillpipeline();
    }
    else
    {
            for (c=0;c<16;c++)
            {
                    if (opcode&mask)
                    {
                            addr+=4;
                            *usrregs[c]=readmeml(addr);
//                            //cycles--;
                    }
                    mask<<=1;
            }
    }
//    //cycles-=3;
    break;


    }
}
#endif
void execarm(int cycs)
{
        int linecyc;
        int target;
        int c;
	uint32_t opcode;
	uint32_t lhs, rhs, dest;
        uint32_t templ,templ2,addr,addr2,mask;
//        int RD;
        cycles+=cycs;
        while (cycles>0)
        {
//                cyccount+=200;
//                linecyc=200;
//                while (linecyc>0)
                for (linecyc=0;linecyc<200;linecyc++)
                {
/*                        oldpc3=oldpc2;
                        oldpc2=oldpc;
                        oldpc=PC;*/

                        if ((PC>>12)!=pccache)
                        {
                                pccache=PC>>12;
                                pccache2=getpccache(PC);
                                if (pccache2==NULL) opcode=pccache=0xFFFFFFFF;
                                else                      opcode=pccache2[PC>>2];
                        }
                        else
                           opcode=pccache2[PC>>2];

                        target=(opcode>>20)&0xFF;
                        if (flaglookup[opcode>>28][(*pcpsr)>>28] && !(armirq&0x80))//prefabort)
                        {
//                                inscounts[(opcode>>20)&0xFF]++;
#ifdef STRONGARM
//                                if ((opcode&0xE000090)==0x90)
//                                {
                                if ((opcode&0xE0000F0)==0xB0) /*LDRH/STRH*/
                                {
                                        error("Bad LDRH/STRH opcode %08X\n",opcode);
                                        dumpregs();
                                        exit(-1);
                                }
                                else if ((opcode&0xE1000D0)==0x1000D0) /*LDRS*/
                                {
                                        error("Bad LDRH/STRH opcode %08X\n",opcode);
                                        dumpregs();
                                        exit(-1);
//                                }
//                                goto domain;
                                }
                                else
                                {
//                                        domain:
//                                        GETRD;
#endif
                                switch (target)//((opcode>>20)&0xFF)
                                {
				case 0x00: /* AND reg */
					if ((opcode & 0xf0) == 0x90) /* MUL */
					{
					      armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
					      if (MULRD==MULRM) armregs[MULRD]=0;
					}
					else
					{
						dest = GETADDR(RN) & shift2(opcode);
						arm_write_dest(opcode, dest);
					}
					break;

				case 0x01: /* ANDS reg */
					if ((opcode & 0xf0) == 0x90) /* MULS */
					{
					        armregs[MULRD]=(armregs[MULRM])*(armregs[MULRS]);
					        if (MULRD==MULRM) armregs[MULRD]=0;
					        setzn(armregs[MULRD]);
					}
					else
					{
					       lhs = GETADDR(RN);
					       if (RD==15)
					       {
					               arm_write_r15(opcode, lhs & shift2(opcode));
					       }
					       else
					       {
						       templ=shift(opcode);
						       armregs[RD] = lhs & templ;
						       setzn(armregs[RD]);
					       }
					}
					break;

				case 0x02: /* EOR reg */
					if ((opcode & 0xf0) == 0x90) /* MLA */
					{
					       armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
					       if (MULRD==MULRM) armregs[MULRD]=0;
					}
					else
					{
						dest = GETADDR(RN) ^ shift2(opcode);
						arm_write_dest(opcode, dest);
                                        }
                                        break;

                                case 0x03: /* EORS reg */
                                        if ((opcode & 0xf0) == 0x90) /* MLAS */
                                        {
                                                armregs[MULRD]=((armregs[MULRM])*(armregs[MULRS]))+armregs[MULRN];
					        if (MULRD==MULRM) armregs[MULRD]=0;
                                                setzn(armregs[MULRD]);
                                        }
                                        else
                                        {
                                                lhs = GETADDR(RN);
                                                if (RD==15)
                                                {
                                                        arm_write_r15(opcode, lhs ^ shift2(opcode));
                                                }
                                                else
                                                {
                                                        templ=shift(opcode);
                                                        armregs[RD] = lhs ^ templ;
                                                        setzn(armregs[RD]);
                                                }
                                        }
                                        break;

                                case 0x04: /* SUB reg */
                                        dest = GETADDR(RN) - shift2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x05: /* SUBS reg */
                                        lhs = GETADDR(RN);
                                        templ=shift2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs - templ);
                                        }
                                        else
                                        {
                                                setsub(lhs, templ, lhs - templ);
                                                armregs[RD] = lhs - templ;
                                        }
                                        break;

                                case 0x06: /* RSB reg */
                                        dest = shift2(opcode) - GETADDR(RN);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x07: /* RSBS reg */
                                        lhs = GETADDR(RN);
                                        templ=shift2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, templ - lhs);
                                        }
                                        else
                                        {
                                                setsub(templ, lhs, templ - lhs);
                                                armregs[RD] = templ - lhs;
                                        }
                                        break;

                                case 0x08: /* ADD reg */
                                #ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* UMULL */
					{
                                                uint64_t mula = (uint64_t) armregs[MULRS];
                                                uint64_t mulb = (uint64_t) armregs[MULRM];
                                                uint64_t mulres = mula * mulb;

                                                armregs[MULRN] = (uint32_t) mulres;
                                                armregs[MULRD] = (uint32_t) (mulres >> 32);
                                        }
                                        else
                                        {
                                #endif
                                        dest = GETADDR(RN) + shift2(opcode);
                                        arm_write_dest(opcode, dest);
                                #ifdef STRONGARM
                                        }
                                #endif
                                        break;

                                case 0x09: /* ADDS reg */
                                #ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* UMULLS */
					{
                                                uint64_t mula = (uint64_t) armregs[MULRS];
                                                uint64_t mulb = (uint64_t) armregs[MULRM];
                                                uint64_t mulres = mula * mulb;

                                                armregs[MULRN] = (uint32_t) mulres;
                                                armregs[MULRD] = (uint32_t) (mulres >> 32);
                                                arm_flags_long_multiply(mulres);
                                        }
                                        else
                                        {
                                #endif
                                        lhs = GETADDR(RN);
                                        templ=shift2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs + templ);
                                        }
                                        else
                                        {
                                                setadd(lhs, templ, lhs + templ);
                                                armregs[RD] = lhs + templ;
                                        }
                                #ifdef STRONGARM                                                                                
                                        }
                                #endif
                                        break;
                                
                                case 0x0A: /* ADC reg */
                                #ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* UMLAL */
					{
                                                uint64_t mula = (uint64_t) armregs[MULRS];
                                                uint64_t mulb = (uint64_t) armregs[MULRM];
                                                uint64_t current = ((uint64_t) armregs[MULRD] << 32) |
                                                                   armregs[MULRN];
                                                uint64_t mulres = (mula * mulb) + current;

                                                armregs[MULRN] = (uint32_t) mulres;
                                                armregs[MULRD] = (uint32_t) (mulres >> 32);
                                        }
                                        else
                                        {
                                #endif
                                        dest = GETADDR(RN) + shift2(opcode) + CFSET;
                                        arm_write_dest(opcode, dest);
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;

                                case 0x0B: /* ADCS reg */
                                #ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* UMLALS */
					{
                                                uint64_t mula = (uint64_t) armregs[MULRS];
                                                uint64_t mulb = (uint64_t) armregs[MULRM];
                                                uint64_t current = ((uint64_t) armregs[MULRD] << 32) |
                                                                   armregs[MULRN];
                                                uint64_t mulres = (mula * mulb) + current;

                                                armregs[MULRN] = (uint32_t) mulres;
                                                armregs[MULRD] = (uint32_t) (mulres >> 32);
                                                arm_flags_long_multiply(mulres);
                                        }
                                        else
                                        {
                                #endif                                                
                                        lhs = GETADDR(RN);
                                        templ2=CFSET;
                                        templ=shift2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs + templ + templ2);
                                        }
                                        else
                                        {
                                                setadc(lhs, templ, lhs + templ + templ2);
                                                armregs[RD] = lhs + templ + templ2;
                                        }
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;

                                case 0x0C: /* SBC reg */
                                #ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* SMULL */
					{
                                                int64_t mula = (int64_t) (int32_t) armregs[MULRS];
                                                int64_t mulb = (int64_t) (int32_t) armregs[MULRM];
                                                int64_t mulres = mula * mulb;

                                                armregs[MULRN] = (uint32_t) mulres;
                                                armregs[MULRD] = (uint32_t) (mulres >> 32);
                                        }
                                        else
                                        {
                                #endif
                                        dest = GETADDR(RN) - shift2(opcode) - ((CFSET) ? 0 : 1);
                                        arm_write_dest(opcode, dest);
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;

                                case 0x0D: /* SBCS reg */
                                #ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* SMULLS */
					{
                                                int64_t mula = (int64_t) (int32_t) armregs[MULRS];
                                                int64_t mulb = (int64_t) (int32_t) armregs[MULRM];
                                                int64_t mulres = mula * mulb;

                                                armregs[MULRN] = (uint32_t) mulres;
                                                armregs[MULRD] = (uint32_t) (mulres >> 32);
                                                arm_flags_long_multiply(mulres);
                                        }
                                        else
                                        {
                                #endif
                                        lhs = GETADDR(RN);
                                        templ2=(CFSET)?0:1;
                                        templ=shift2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs - (templ + templ2));
                                        }
                                        else
                                        {
                                                setsbc(lhs, templ, lhs - (templ + templ2));
                                                armregs[RD] = lhs - (templ + templ2);
                                        }
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;

                                case 0x0E: /* RSC reg */
                                #ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* SMLAL */
					{
                                                int64_t mula = (int64_t) (int32_t) armregs[MULRS];
                                                int64_t mulb = (int64_t) (int32_t) armregs[MULRM];
                                                int64_t current = ((int64_t) armregs[MULRD] << 32) |
                                                                   armregs[MULRN];
                                                int64_t mulres = (mula * mulb) + current;

                                                armregs[MULRN] = (uint32_t) mulres;
                                                armregs[MULRD] = (uint32_t) (mulres >> 32);
                                        }
                                        else
                                        {
                                #endif
                                        dest = shift2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
                                        arm_write_dest(opcode, dest);
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;

                                case 0x0F: /* RSCS reg */
                                #ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* SMLALS */
					{
                                                int64_t mula = (int64_t) (int32_t) armregs[MULRS];
                                                int64_t mulb = (int64_t) (int32_t) armregs[MULRM];
                                                int64_t current = ((int64_t) armregs[MULRD] << 32) |
                                                                   armregs[MULRN];
                                                int64_t mulres = (mula * mulb) + current;

                                                armregs[MULRN] = (uint32_t) mulres;
                                                armregs[MULRD] = (uint32_t) (mulres >> 32);
                                                arm_flags_long_multiply(mulres);
                                        }
                                        else
                                        {
                                #endif
                                        lhs = GETADDR(RN);
                                        templ2=(CFSET)?0:1;
                                        templ=shift2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, templ - (lhs + templ2));
                                        }
                                        else
                                        {
                                                setsbc(templ, lhs, templ - (lhs + templ2));
                                                armregs[RD] = templ - (lhs + templ2);
                                        }
                                #ifdef STRONGARM                                        
                                        }
                                #endif
                                        break;

                                case 0x10: /* MRS reg,CPSR and SWP word */
                                        if ((opcode&0xF0)==0x90)
                                        {
                                                addr=armregs[RN];
                                                templ=GETREG(RM);
                                                LOADREG(RD,readmeml(addr));
                                                writememl(addr,templ);
                                        }
                                        else if (!(opcode&0xFFF)) /*MRS CPSR*/
                                        {
                                                if (!(mode&16))
                                                {
                                                        armregs[16]=(armregs[15]&0xF0000000)|(armregs[15]&3);
                                                        armregs[16]|=((armregs[15]&0xC000000)>>20);
                                                }
                                                armregs[RD]=armregs[16];
                                        }
                                        else
                                        {
                                                undefined();
//					        bad_opcode(opcode);
                                        }
                                        break;
                                        
                                case 0x11: /* TST reg */
                                        lhs = GETADDR(RN);
                                        if (RD==15)
                                        {
                                                /* TSTP reg */
                                                arm_compare_rd15(opcode, lhs & shift2(opcode));
                                        }
                                        else
                                        {
                                                setzn(lhs & shift(opcode));
                                        }
                                        break;

				case 0x12: /* MSR CPSR, reg */
					if ((RD == 15) && ((opcode & 0xff0) == 0)) {
						arm_write_cpsr(opcode, armregs[RM]);
					} else {
						bad_opcode(opcode);
					}
					break;
                                        
                                case 0x13: /* TEQ reg */
                                        lhs = GETADDR(RN);
                                        if (RD==15)
                                        {
                                                /* TEQP reg */
                                                arm_compare_rd15(opcode, lhs ^ shift2(opcode));
                                        }
                                        else
                                        {
                                                setzn(lhs ^ shift(opcode));
                                        }
                                        break;

                                case 0x14: /* MRS reg,SPSR and SWPB */
                                        if ((opcode&0xF0)==0x90) /* SWPB */
                                        {
                                                addr=armregs[RN];
                                                templ=GETREG(RM);
                                                LOADREG(RD,readmemb(addr));
                                                writememb(addr,templ);
                                        } else if (!(opcode&0xFFF)) /* MRS SPSR */
                                        {
                                                armregs[RD]=spsr[mode&15];
                                        }
                                        else
                                        {
						bad_opcode(opcode);
                                        }
                                        break;
                                        
                                case 0x15: /* CMP reg */
                                        lhs = GETADDR(RN);
                                        rhs = shift2(opcode);
                                        dest = lhs - rhs;
                                        if (RD==15)
                                        {
                                                /* CMPP reg */
                                                arm_compare_rd15(opcode, dest);
                                        }
                                        else
                                        {
                                                setsub(lhs, rhs, dest);
                                        }
                                        break;

                                case 0x16: /* MSR SPSR, reg */
					if ((RD == 15) && ((opcode & 0xff0) == 0)) {
						arm_write_spsr(opcode, armregs[RM]);
					} else {
						bad_opcode(opcode);
					}
					break;

                                case 0x17: /* CMN reg */
                                        lhs = GETADDR(RN);
                                        rhs = shift2(opcode);
                                        dest = lhs + rhs;
                                        if (RD==15)
                                        {
                                                /* CMNP reg */
                                                arm_compare_rd15(opcode, dest);
                                        } else {
                                                setadd(lhs, rhs, dest);
                                        }
                                        break;

                                case 0x18: /* ORR reg */
                                        dest = GETADDR(RN) | shift2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x19: /* ORRS reg */
                                        lhs = GETADDR(RN);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs | shift2(opcode));
                                        }
                                        else
                                        {
                                                templ=shift(opcode);
                                                armregs[RD] = lhs | templ;
                                                setzn(armregs[RD]);
                                        }
                                        break;

                                case 0x1A: /* MOV reg */
                                        dest = shift2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x1B: /* MOVS reg */
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, shift2(opcode));
                                        }
                                        else
                                        {
                                                armregs[RD]=shift(opcode);
                                                setzn(armregs[RD]);
                                        }
                                        break;

                                case 0x1C: /* BIC reg */
                                        dest = GETADDR(RN) & ~shift2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x1D: /* BICS reg */
                                        lhs = GETADDR(RN);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs & ~shift2(opcode));
                                        }
                                        else
                                        {
                                                templ=shift(opcode);
                                                armregs[RD] = lhs & ~templ;
                                                setzn(armregs[RD]);
                                        }
                                        break;

                                case 0x1E: /* MVN reg */
                                        dest = ~shift2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x1F: /* MVNS reg */
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, ~shift2(opcode));
                                        }
                                        else
                                        {
                                                armregs[RD]=~shift(opcode);
                                                setzn(armregs[RD]);
                                        }
                                        break;

                                case 0x20: /* AND imm */
                                        dest = GETADDR(RN) & rotate2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x21: /* ANDS imm */
                                        lhs = GETADDR(RN);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs & rotate2(opcode));
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                armregs[RD] = lhs & templ;
                                                setzn(armregs[RD]);
                                        }
                                        break;

                                case 0x22: /* EOR imm */
                                        dest = GETADDR(RN) ^ rotate2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x23: /* EORS imm */
                                        lhs = GETADDR(RN);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs ^ rotate2(opcode));
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                armregs[RD] = lhs ^ templ;
                                                setzn(armregs[RD]);
                                        }
                                        break;

                                case 0x24: /* SUB imm */
                                        dest = GETADDR(RN) - rotate2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x25: /* SUBS imm */
                                        lhs = GETADDR(RN);
                                        templ=rotate2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs - templ);
                                        }
                                        else
                                        {
                                                armregs[RD] = lhs - templ;
                                                setsub(lhs, templ, lhs - templ);
                                        }
                                        break;

                                case 0x26: /* RSB imm */
                                        dest = rotate2(opcode) - GETADDR(RN);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x27: /* RSBS imm */
                                        lhs = GETADDR(RN);
                                        templ=rotate2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, templ - lhs);
                                        }
                                        else
                                        {
                                                setsub(templ, lhs, templ - lhs);
                                                armregs[RD] = templ - lhs;
                                        }
                                        break;

                                case 0x28: /* ADD imm */
                                        dest = GETADDR(RN) + rotate2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x29: /* ADDS imm */
                                        lhs = GETADDR(RN);
                                        templ=rotate2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs + templ);
                                        }
                                        else
                                        {
                                                setadd(lhs, templ, lhs + templ);
                                                armregs[RD] = lhs + templ;
                                        }
                                        break;

                                case 0x2A: /* ADC imm */
                                        dest = GETADDR(RN) + rotate2(opcode) + CFSET;
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x2B: /* ADCS imm */
                                        lhs = GETADDR(RN);
                                        templ2=CFSET;
                                        templ=rotate2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs + templ + templ2);
                                        }
                                        else
                                        {
                                                setadc(lhs, templ, lhs + templ + templ2);
                                                armregs[RD] = lhs + templ + templ2;
                                        }
                                        break;

                                case 0x2C: /* SBC imm */
                                        dest = GETADDR(RN) - rotate2(opcode) - ((CFSET) ? 0 : 1);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x2D: /* SBCS imm */
                                        lhs = GETADDR(RN);
                                        templ2=(CFSET)?0:1;
                                        templ=rotate2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs - (templ + templ2));
                                        }
                                        else
                                        {
                                                setsbc(lhs, templ, lhs - (templ + templ2));
                                                armregs[RD] = lhs - (templ + templ2);
                                        }
                                        break;

                                case 0x2E: /* RSC imm */
                                        dest = rotate2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x2F: /* RSCS imm */
                                        lhs = GETADDR(RN);
                                        templ2=(CFSET)?0:1;
                                        templ=rotate2(opcode);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, templ - (lhs + templ2));
                                        }
                                        else
                                        {
                                                setsbc(templ, lhs, templ - (lhs + templ2));
                                                armregs[RD] = templ - (lhs + templ2);
                                        }
                                        break;

                                case 0x31: /* TST imm */
                                        lhs = GETADDR(RN);
                                        if (RD==15)
                                        {
                                                /* TSTP imm */
                                                arm_compare_rd15(opcode, lhs & rotate2(opcode));
                                        }
                                        else
                                        {
                                                setzn(lhs & rotate(opcode));
                                        }
                                        break;

				case 0x32: /* MSR CPSR, imm */
					if (RD == 15) {
						arm_write_cpsr(opcode, rotate2(opcode));
					} else {
						bad_opcode(opcode);
					}
					break;

                                case 0x33: /* TEQ imm */
                                        lhs = GETADDR(RN);
                                        if (RD==15)
                                        {
                                                /* TEQP imm */
                                                arm_compare_rd15(opcode, lhs ^ rotate2(opcode));
                                        }
                                        else
                                        {
                                                setzn(lhs ^ rotate(opcode));
                                        }
                                        break;

                                case 0x35: /* CMP imm */
                                        lhs = GETADDR(RN);
                                        rhs = rotate2(opcode);
                                        dest = lhs - rhs;
                                        if (RD==15)
                                        {
                                                /* CMPP imm */
                                                arm_compare_rd15(opcode, dest);
                                        }
                                        else
                                        {
                                                setsub(lhs, rhs, dest);
                                        }
                                        break;

                                case 0x37: /* CMN imm */
                                        lhs = GETADDR(RN);
                                        rhs = rotate2(opcode);
                                        dest = lhs + rhs;
                                        if (RD==15)
                                        {
                                                /* CMNP imm */
                                                arm_compare_rd15(opcode, dest);
                                        } else {
                                                setadd(lhs, rhs, dest);
                                        }
                                        break;

                                case 0x38: /* ORR imm */
                                        dest = GETADDR(RN) | rotate2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x39: /* ORRS imm */
                                        lhs = GETADDR(RN);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs | rotate2(opcode));
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                armregs[RD] = lhs | templ;
                                                setzn(armregs[RD]);
                                        }
                                        break;

                                case 0x3A: /* MOV imm */
                                        dest = rotate2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x3B: /* MOVS imm */
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, rotate2(opcode));
                                        }
                                        else
                                        {
                                                armregs[RD]=rotate(opcode);
                                                setzn(armregs[RD]);
                                        }
                                        break;

                                case 0x3C: /* BIC imm */
                                        dest = GETADDR(RN) & ~rotate2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x3D: /* BICS imm */
                                        lhs = GETADDR(RN);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, lhs & ~rotate2(opcode));
                                        }
                                        else
                                        {
                                                templ=rotate(opcode);
                                                armregs[RD] = lhs & ~templ;
                                                setzn(armregs[RD]);
                                        }
                                        break;

                                case 0x3E: /* MVN imm */
                                        dest = ~rotate2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x3F: /* MVNS imm */
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, ~rotate2(opcode));
                                        }
                                        else
                                        {
                                                armregs[RD]=~rotate(opcode);
                                                setzn(armregs[RD]);
                                        }
                                        break;
//#endif

				case 0x42: /* STRT Rd, [Rn], #-imm   */
				case 0x4a: /* STRT Rd, [Rn], #+imm   */
				case 0x62: /* STRT Rd, [Rn], -reg... */
				case 0x6a: /* STRT Rd, [Rn], +reg... */
					addr = GETADDR(RN);

					/* Temp switch to user permissions */
					templ = memmode;
					memmode = 0;
					writememl(addr & ~3, armregs[RD]);
					memmode = templ;

					/* Check for Abort */
					if (armirq & 0x40)
						break;

					/* Writeback */
					if (opcode & 0x2000000) {
						addr2 = shift_ldrstr(opcode);
					} else {
						addr2 = opcode & 0xfff;
					}
					if (!(opcode & 0x800000)) {
						addr2 = -addr2;
					}
					addr += addr2;
					armregs[RN] = addr;
					break;

				case 0x43: /* LDRT Rd, [Rn], #-imm   */
				case 0x4b: /* LDRT Rd, [Rn], #+imm   */
				case 0x63: /* LDRT Rd, [Rn], -reg... */
				case 0x6b: /* LDRT Rd, [Rn], +reg... */
					addr = GETADDR(RN);

					/* Temp switch to user permissions */
					templ = memmode;
					memmode = 0;
					templ2 = readmeml(addr & ~3);
					memmode = templ;

					/* Check for Abort */
					if (armirq & 0x40)
						break;

					/* Rotate if load is unaligned */
					if (addr & 3) {
						templ2 = ldrresult(templ2, addr);
					}

					/* Writeback */
					if (opcode & 0x2000000) {
						addr2 = shift_ldrstr(opcode);
					} else {
						addr2 = opcode & 0xfff;
					}
					if (!(opcode & 0x800000)) {
						addr2 = -addr2;
					}
					addr += addr2;
					armregs[RN] = addr;

					/* Write Rd */
					LOADREG(RD, templ2);
					break;

				case 0x46: /* STRBT Rd, [Rn], #-imm   */
				case 0x4e: /* STRBT Rd, [Rn], #+imm   */
				case 0x66: /* STRBT Rd, [Rn], -reg... */
				case 0x6e: /* STRBT Rd, [Rn], +reg... */
					addr = GETADDR(RN);

					/* Temp switch to user permissions */
					templ = memmode;
					memmode = 0;
					writememb(addr, armregs[RD]);
					memmode = templ;

					/* Check for Abort */
					if (armirq & 0x40)
						break;

					/* Writeback */
					if (opcode & 0x2000000) {
						addr2 = shift_ldrstr(opcode);
					} else {
						addr2 = opcode & 0xfff;
					}
					if (!(opcode & 0x800000)) {
						addr2 = -addr2;
					}
					addr += addr2;
					armregs[RN] = addr;
					break;

				case 0x47: /* LDRBT Rd, [Rn], #-imm   */
				case 0x4f: /* LDRBT Rd, [Rn], #+imm   */
				case 0x67: /* LDRBT Rd, [Rn], -reg... */
				case 0x6f: /* LDRBT Rd, [Rn], +reg... */
					addr = GETADDR(RN);

					/* Temp switch to user permissions */
					templ = memmode;
					memmode = 0;
					templ2 = readmemb(addr);
					memmode = templ;

					/* Check for Abort */
					if (armirq & 0x40)
						break;

					/* Writeback */
					if (opcode & 0x2000000) {
						addr2 = shift_ldrstr(opcode);
					} else {
						addr2 = opcode & 0xfff;
					}
					if (!(opcode & 0x800000)) {
						addr2 = -addr2;
					}
					addr += addr2;
					armregs[RN] = addr;

					/* Write Rd */
					LOADREG(RD, templ2);
					break;

				case 0x60: /* STR Rd, [Rn], -reg...  */
				case 0x68: /* STR Rd, [Rn], +reg...  */
				case 0x70: /* STR Rd, [Rn, -reg...]  */
				case 0x72: /* STR Rd, [Rn, -reg...]! */
				case 0x78: /* STR Rd, [Rn, +reg...]  */
				case 0x7a: /* STR Rd, [Rn, +reg...]! */
					if (opcode & 0x10) {
						undefined();
						break;
					}
					/* Fall-through */
				case 0x40: /* STR Rd, [Rn], #-imm  */
				case 0x48: /* STR Rd, [Rn], #+imm  */
				case 0x50: /* STR Rd, [Rn, #-imm]  */
				case 0x52: /* STR Rd, [Rn, #-imm]! */
				case 0x58: /* STR Rd, [Rn, #+imm]  */
				case 0x5a: /* STR Rd, [Rn, #+imm]! */
					addr = GETADDR(RN);

					/* Calculate offset */
					if (opcode & 0x2000000) {
						addr2 = shift_ldrstr(opcode);
					} else {
						addr2 = opcode & 0xfff;
					}
					if (!(opcode & 0x800000)) {
						addr2 = -addr2;
					}

					/* Pre-indexed */
					if (opcode & 0x1000000) {
						addr += addr2;
					}

					/* Store */
					templ = armregs[RD];
					if (RD == 15) {
						templ += r15diff;
					}
					writememl(addr & ~3, templ);

					/* Check for Abort */
					if (armirq & 0x40)
						break;

					if (!(opcode & 0x1000000)) {
						/* Post-indexed */
						addr += addr2;
						armregs[RN] = addr;
					} else if (opcode & 0x200000) {
						/* Pre-indexed with writeback */
						armregs[RN] = addr;
					}
					break;

				case 0x61: /* LDR Rd, [Rn], -reg...  */
				case 0x69: /* LDR Rd, [Rn], +reg...  */
				case 0x71: /* LDR Rd, [Rn, -reg...]  */
				case 0x73: /* LDR Rd, [Rn, -reg...]! */
				case 0x79: /* LDR Rd, [Rn, +reg...]  */
				case 0x7b: /* LDR Rd, [Rn, +reg...]! */
					if (opcode & 0x10) {
						undefined();
						break;
					}
					/* Fall-through */
				case 0x41: /* LDR Rd, [Rn], #-imm  */
				case 0x49: /* LDR Rd, [Rn], #+imm  */
				case 0x51: /* LDR Rd, [Rn, #-imm]  */
				case 0x53: /* LDR Rd, [Rn, #-imm]! */
				case 0x59: /* LDR Rd, [Rn, #+imm]  */
				case 0x5b: /* LDR Rd, [Rn, #+imm]! */
					addr = GETADDR(RN);

					/* Calculate offset */
					if (opcode & 0x2000000) {
						addr2 = shift_ldrstr(opcode);
					} else {
						addr2 = opcode & 0xfff;
					}
					if (!(opcode & 0x800000)) {
						addr2 = -addr2;
					}

					/* Pre-indexed */
					if (opcode & 0x1000000) {
						addr += addr2;
					}

					/* Load */
					templ = readmeml(addr & ~3);

					/* Check for Abort */
					if (armirq & 0x40)
						break;

					/* Rotate if load is unaligned */
					if (addr & 3) {
						templ = ldrresult(templ, addr);
					}

					if (!(opcode & 0x1000000)) {
						/* Post-indexed */
						addr += addr2;
						armregs[RN] = addr;
					} else if (opcode & 0x200000) {
						/* Pre-indexed with writeback */
						armregs[RN] = addr;
					}

					/* Write Rd */
					LOADREG(RD, templ);
					break;

				case 0x64: /* STRB Rd, [Rn], -reg...  */
				case 0x6c: /* STRB Rd, [Rn], +reg...  */
				case 0x74: /* STRB Rd, [Rn, -reg...]  */
				case 0x76: /* STRB Rd, [Rn, -reg...]! */
				case 0x7c: /* STRB Rd, [Rn, +reg...]  */
				case 0x7e: /* STRB Rd, [Rn, +reg...]! */
					if (opcode & 0x10) {
						undefined();
						break;
					}
					/* Fall-through */
				case 0x44: /* STRB Rd, [Rn], #-imm  */
				case 0x4c: /* STRB Rd, [Rn], #+imm  */
				case 0x54: /* STRB Rd, [Rn, #-imm]  */
				case 0x56: /* STRB Rd, [Rn, #-imm]! */
				case 0x5c: /* STRB Rd, [Rn, #+imm]  */
				case 0x5e: /* STRB Rd, [Rn, #+imm]! */
					addr = GETADDR(RN);

					/* Calculate offset */
					if (opcode & 0x2000000) {
						addr2 = shift_ldrstr(opcode);
					} else {
						addr2 = opcode & 0xfff;
					}
					if (!(opcode & 0x800000)) {
						addr2 = -addr2;
					}

					/* Pre-indexed */
					if (opcode & 0x1000000) {
						addr += addr2;
					}

					/* Store */
					writememb(addr, armregs[RD]);

					/* Check for Abort */
					if (armirq & 0x40)
						break;

					if (!(opcode & 0x1000000)) {
						/* Post-indexed */
						addr += addr2;
						armregs[RN] = addr;
					} else if (opcode & 0x200000) {
						/* Pre-indexed with writeback */
						armregs[RN] = addr;
					}
					break;

				case 0x65: /* LDRB Rd, [Rn], -reg... */
				case 0x6d: /* LDRB Rd, [Rn], +reg...  */
				case 0x75: /* LDRB Rd, [Rn, -reg...]  */
				case 0x77: /* LDRB Rd, [Rn, -reg...]! */
				case 0x7d: /* LDRB Rd, [Rn, +reg...]  */
				case 0x7f: /* LDRB Rd, [Rn, +reg...]! */
					if (opcode & 0x10) {
						undefined();
						break;
					}
					/* Fall-through */
				case 0x45: /* LDRB Rd, [Rn], #-imm  */
				case 0x4d: /* LDRB Rd, [Rn], #+imm  */
				case 0x55: /* LDRB Rd, [Rn, #-imm]  */
				case 0x57: /* LDRB Rd, [Rn, #-imm]! */
				case 0x5d: /* LDRB Rd, [Rn, #+imm]  */
				case 0x5f: /* LDRB Rd, [Rn, #+imm]! */
					addr = GETADDR(RN);

					/* Calculate offset */
					if (opcode & 0x2000000) {
						addr2 = shift_ldrstr(opcode);
					} else {
						addr2 = opcode & 0xfff;
					}
					if (!(opcode & 0x800000)) {
						addr2 = -addr2;
					}

					/* Pre-indexed */
					if (opcode & 0x1000000) {
						addr += addr2;
					}

					/* Load */
					templ = readmemb(addr);

					/* Check for Abort */
					if (armirq & 0x40)
						break;

					if (!(opcode & 0x1000000)) {
						/* Post-indexed */
						addr += addr2;
						armregs[RN] = addr;
					} else if (opcode & 0x200000) {
						/* Pre-indexed with writeback */
						armregs[RN] = addr;
					}

					/* Write Rd */
					LOADREG(RD, templ);
					break;

/*                                        case 0x80: case 0x81: case 0x82: case 0x83:
                                        case 0x84: case 0x85: case 0x86: case 0x87:
                                        case 0x88: case 0x89: case 0x8A: case 0x8B:
                                        case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                                        case 0x90: case 0x91: case 0x92: case 0x93:
                                        case 0x94: case 0x95: case 0x96: case 0x97:
                                        case 0x98: case 0x99: case 0x9A: case 0x9B:
                                        case 0x9C: case 0x9D: case 0x9E: case 0x9F:
                                        ldmstm((opcode>>20)&0xFF, opcode);
                                        break;*/

#define STMfirst()      mask=1; \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        writememl(addr, armregs[c]); \
                                        addr+=4; \
                                        break; \
                                } \
                                mask<<=1; \
                        } \
                        mask<<=1; c++;

#define STMall()        for (;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        writememl(addr,armregs[c]); \
                                        addr+=4; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                writememl(addr,armregs[15]+r15diff); \
                        }

#define STMfirstS()     mask=1; \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        writememl(addr, *usrregs[c]); \
                                        addr+=4; \
                                        break; \
                                } \
                                mask<<=1; \
                        } \
                        mask<<=1; c++;

#define STMallS()       for (;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        writememl(addr,*usrregs[c]); \
                                        addr+=4; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                writememl(addr,armregs[15]+r15diff); \
                        }

#define LDMall()        mask=1; \
                        for (c=0;c<15;c++) \
                        { \
                                if (opcode&mask) \
                                { \
                                        armregs[c]=readmeml(addr); \
                                        addr+=4; \
                                } \
                                mask<<=1; \
                        } \
                        if (opcode&0x8000) \
                        { \
                                armregs[15]=(armregs[15]&~r15mask)|((readmeml(addr)+4)&r15mask); \
                                refillpipeline(); \
                        }

#define LDMallS()       mask=1; \
                        if (opcode&0x8000) \
                        { \
                                for (c=0;c<15;c++) \
                                { \
                                        if (opcode&mask) \
                                        { \
                                                armregs[c]=readmeml(addr); \
                                                addr+=4; \
                                        } \
                                        mask<<=1; \
                                } \
                                if ((armregs[15]&3) || (mode&16)) armregs[15]=(readmeml(addr)+4); \
                                else                              armregs[15]=(armregs[15]&0x0C000003)|((readmeml(addr)+4)&0xF3FFFFFC); \
                                if (mode&16) armregs[cpsr]=spsr[mode&15]; \
                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask); \
                                refillpipeline(); \
                        } \
                        else \
                        { \
                                for (c=0;c<15;c++) \
                                { \
                                        if (opcode&mask) \
                                        { \
                                                *usrregs[c]=readmeml(addr); \
                                                addr+=4; \
                                        } \
                                        mask<<=1; \
                                } \
                        }

				case 0x80: /* STMDA */
				case 0x82: /* STMDA ! */
				case 0x90: /* STMDB */
				case 0x92: /* STMDB ! */
                                        templ=armregs[RN];
                                        addr=(armregs[RN]&~3)-countbits(opcode&0xFFFF);
                                        if (!(opcode&0x1000000)) addr+=4;
                                        STMfirst();
                                        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
                                        STMall()
                                        if (armirq&0x40) armregs[RN]=templ;
                                        break;

				case 0x88: /* STMIA */
				case 0x8a: /* STMIA ! */
				case 0x98: /* STMIB */
				case 0x9a: /* STMIB ! */
                                        templ=armregs[RN];
                                        addr=armregs[RN]&~3;
                                        if (opcode&0x1000000) addr+=4;
                                        STMfirst();
                                        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
                                        STMall();
                                        if (armirq&0x40) armregs[RN]=templ;
                                        break;

				case 0x84: /* STMDA ^ */
				case 0x86: /* STMDA ^! */
				case 0x94: /* STMDB ^ */
				case 0x96: /* STMDB ^! */
                                        templ=armregs[RN];
                                        addr=(armregs[RN]&~3)-countbits(opcode&0xFFFF);
                                        if (!(opcode&0x1000000)) addr+=4;
                                        STMfirstS();
                                        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
                                        STMallS()
                                        if (armirq&0x40) armregs[RN]=templ;
                                        break;

				case 0x8c: /* STMIA ^ */
				case 0x8e: /* STMIA ^! */
				case 0x9c: /* STMIB ^ */
				case 0x9e: /* STMIB ^! */
                                        templ=armregs[RN];
                                        addr=armregs[RN]&~3;
                                        if (opcode&0x1000000) addr+=4;
                                        STMfirstS();
                                        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
                                        STMallS();
                                        if (armirq&0x40) armregs[RN]=templ;
                                        break;

				case 0x81: /* LDMDA */
				case 0x83: /* LDMDA ! */
				case 0x91: /* LDMDB */
				case 0x93: /* LDMDB ! */
                                        templ=armregs[RN];
                                        addr=(armregs[RN]&~3)-countbits(opcode&0xFFFF);
                                        if (!(opcode&0x1000000)) addr+=4;
                                        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
                                        LDMall();
                                        if (armirq&0x40) armregs[RN]=templ;
                                        break;

				case 0x89: /* LDMIA */
				case 0x8b: /* LDMIA ! */
				case 0x99: /* LDMIB */
				case 0x9b: /* LDMIB ! */
                                        templ=armregs[RN];
                                        addr=armregs[RN]&~3;
                                        if (opcode&0x1000000) addr+=4;
                                        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
                                        LDMall();
                                        if (armirq&0x40) armregs[RN]=templ;
                                        break;

				case 0x85: /* LDMDA ^ */
				case 0x87: /* LDMDA ^! */
				case 0x95: /* LDMDB ^ */
				case 0x97: /* LDMDB ^! */
                                        templ=armregs[RN];
                                        addr=(armregs[RN]&~3)-countbits(opcode&0xFFFF);
                                        if (!(opcode&0x1000000)) addr+=4;
                                        if (opcode&0x200000) armregs[RN]-=countbits(opcode&0xFFFF);
                                        LDMallS();
                                        if (armirq&0x40) armregs[RN]=templ;
                                        break;

				case 0x8d: /* LDMIA ^ */
				case 0x8f: /* LDMIA ^! */
				case 0x9d: /* LDMIB ^ */
				case 0x9f: /* LDMIB ^! */
                                        templ=armregs[RN];
                                        addr=armregs[RN]&~3;
                                        if (opcode&0x1000000) addr+=4;
                                        if (opcode&0x200000) armregs[RN]+=countbits(opcode&0xFFFF);
                                        LDMallS();
                                        if (armirq&0x40) armregs[RN]=templ;
                                        break;

                                case 0xA0: case 0xA1: case 0xA2: case 0xA3: /* B */
                                case 0xA4: case 0xA5: case 0xA6: case 0xA7:
                                case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                                case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                                        /* Extract offset bits, and sign-extend */
                                        templ = (opcode << 8);
                                        templ = (uint32_t) ((int32_t) templ >> 6);
                                        armregs[15]=((armregs[15]+templ+4)&r15mask)|(armregs[15]&~r15mask);
                                        break;

                                case 0xB0: case 0xB1: case 0xB2: case 0xB3: /* BL */
                                case 0xB4: case 0xB5: case 0xB6: case 0xB7:
                                case 0xB8: case 0xB9: case 0xBA: case 0xBB:
                                case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                                        /* Extract offset bits, and sign-extend */
                                        templ = (opcode << 8);
                                        templ = (uint32_t) ((int32_t) templ >> 6);
                                        armregs[14]=armregs[15]-4;
                                        armregs[15]=((armregs[15]+templ+4)&r15mask)|(armregs[15]&~r15mask);
                                        refillpipeline();
                                        break;

                                        case 0xE0: case 0xE2: case 0xE4: case 0xE6: /*MCR*/
                                        case 0xE8: case 0xEA: case 0xEC: case 0xEE:
#ifdef FPA
                                        if (MULRS==1)
                                        {
                                                fpaopcode(opcode);
                                        }
                                        else
#endif
                                        if (MULRS==15 && (opcode&0x10))
                                        {
                                                writecp15(RN,armregs[RD],opcode);
                                        }
                                        else
                                        {
                                                undefined();
                                        }
                                        break;

                                        case 0xE1: case 0xE3: case 0xE5: case 0xE7: /*MRC*/
                                        case 0xE9: case 0xEB: case 0xED: case 0xEF:
#ifdef FPA
                                        if (MULRS==1)
                                        {
                                                fpaopcode(opcode);
                                        }
                                        else
#endif
                                        if (MULRS==15 && (opcode&0x10))
                                        {
                                                if (RD==15) armregs[RD]=(armregs[RD]&r15mask)|(readcp15(RN)&~r15mask);
                                                else        armregs[RD]=readcp15(RN);
                                        }
                                        else
                                        {
                                                undefined();
                                        }
                                        break;

//#if 0
                                        case 0xC0: case 0xC1: case 0xC2: case 0xC3: /*Co-pro*/
                                        case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                                        case 0xC8: case 0xC9: case 0xCA: case 0xCB:
                                        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                                        case 0xD0: case 0xD1: case 0xD2: case 0xD3:
                                        case 0xD4: case 0xD5: case 0xD6: case 0xD7:
                                        case 0xD8: case 0xD9: case 0xDA: case 0xDB:
                                        case 0xDC: case 0xDD: case 0xDE: case 0xDF:
#ifdef FPA
                                        if ((opcode&0xF00)==0x100 || (opcode&0xF00)==0x200)
                                           fpaopcode(opcode);
                                        else
                                        {
                                                undefined();
                                        }
#else
                                        undefined();
#endif
                                        break;
//#endif
                                        case 0xF0: case 0xF1: case 0xF2: case 0xF3: /*SWI*/
                                        case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                                        case 0xF8: case 0xF9: case 0xFA: case 0xFB:
                                        case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                                        templ=opcode&0xDFFFF;
                                        if (mousehack && templ==7 && armregs[0]==0x15)
                                        {
//                                                printf("OSWORD call %i\n",readmemb(armregs[1]));
                                                if (readmemb(armregs[1])==1)
                                                {
                                                        setmouseparams(armregs[1]);
                                                        break;
                                                }
                                                else if (readmemb(armregs[1])==4)
                                                {
                                                        getunbufmouse(armregs[1]);
                                                        break;
                                                }
                                                else if (readmemb(armregs[1])==3)
                                                {
                                                        setmousepos(armregs[1]);
                                                        break;
                                                }
                                                else if (readmemb(armregs[1])==5)
                                                {
                                                        setmousepos(armregs[1]);
                                                        break;
                                                }
                                                else
                                                  goto realswi;
                                        }
                                        else if (mousehack && templ==0x1C)
                                        {
                                                getosmouse();
                                                armregs[15]&=~VFLAG;
                                        }
                                        else if (templ == ARCEM_SWI_HOSTFS)
					  {
					    ARMul_State state;
					    state.Reg = armregs;
					    hostfs(&state);
					  }
                                        else if (templ == ARCEM_SWI_NANOSLEEP)
                                          {
#ifdef RPCEMU_WIN
                                            Sleep(armregs[0]/1000000);
#else
                                            struct timespec tm;
                                            tm.tv_sec = 0;
                                            tm.tv_nsec = armregs[0];
                                            nanosleep(&tm, NULL);
#endif
                                            armregs[15]&=~VFLAG;
                                          }
#ifdef RPCEMU_LINUX
                                        else if (templ == ARCEM_SWI_NETWORK)
                                          {
                                              networkswi(armregs[0], armregs[1], armregs[2], armregs[3], armregs[4], armregs[5], &armregs[0], &armregs[1]);
                                          }
#endif
					else
					  {
                                                        realswi:
                                                        if (mousehack && templ==7 && armregs[0]==0x15 && readmemb(armregs[1])==0)
                                                           setpointer(armregs[1]);
                                                        if (mousehack && templ==6 && armregs[0]==106)
                                                           osbyte106(armregs[1]);

                                                exception(SUPERVISOR, 0xc, 4);
                                        }
                                        break;

				          default:
					    {
/*					      int ls_opcode = (opcode >> 20) & 0xff;

					      if (ls_opcode >= 0x80 && ls_opcode <= 0xa0)
					        ldmstm(ls_opcode, opcode);
                                              else*/
					        bad_opcode(opcode);
					    }
                                }
                        }
#ifdef STRONGARM
                        }
#endif
                        if (/*databort|*/armirq)//|prefabort)
                        {
                                if (!(mode&16))
                                {
                                        armregs[16]&=~0xC0;
                                        armregs[16]|=((armregs[15]&0xC000000)>>20);
                                }

                                if (armirq&0xC0)
                                {
//                                        exception(ABORT,(armirq&0x80)?0x10:0x14,0);
//                                        armirq&=~0xC0;
//                                        #if 0
                                if (armirq&0x80)//prefabort)       /*Prefetch abort*/
                                {
                                        armirq&=~0xC0;
                                        exception(ABORT, 0x10, 4);
                                }
                                else if (armirq&0x40)//databort==1)     /*Data abort*/
                                {
                                        armirq&=~0xC0;
                                        exception(ABORT, 0x14, 0);
                                }
                                else if (databort==2) /*Address Exception*/
                                {
                                error("Exception %i %i %i\n",databort,armirq,prefabort);
                                dumpregs();
                                exit(-1);
                                        templ=armregs[15];
                                        armregs[15]|=3;
                                        updatemode(SUPERVISOR);
                                        armregs[14]=templ;
                                        armregs[15]&=0xFC000003;
                                        armregs[15]|=0x08000018;
                                        refillpipeline();
                                        databort=0;
                                }
//                                #endif
                                }
                                else if ((armirq&2) && !(armregs[16]&0x40)) /*FIQ*/
                                {
                                        exception(FIQ,0x20,0);
                                }
                                else if ((armirq&1) && !(armregs[16]&0x80)) /*IRQ*/
                                {
                                        exception(IRQ, 0x1c, 0);
                                }
//                                if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                        }
//                        armirq=irq;
                        armregs[15]+=4;

//                        ins++;
//                        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);

//                        linecyc--;
//                        inscount++;
//                        ins++;
                }
                inscount+=200;
                rinscount+=200;

                if (kcallback)
                {
                        kcallback--;
                        if (kcallback<=0)
                        {
                                kcallback=0;
                                keycallback();
                        }
                }
                if (mcallback)
                {
                        mcallback-=10;
                        if (mcallback<=0)
                        {
                                mcallback=0;
                                mscallback();
                        }
                }
                if (fdccallback)
                {
                        fdccallback-=10;
                        if (fdccallback<=0)
                        {
                                fdccallback=0;
                                callbackfdc();
                        }
                }
                if (idecallback)
                {
                        idecallback-=10;
                        if (idecallback<=0)
                        {
                                idecallback=0;
                                callbackide();
                        }
                }
                if (motoron)
                {
                        fdci--;
                        if (fdci<=0)
                        {
                                fdci=20000;
                                iomd.irqa.status |= IOMD_IRQA_FLOPPY_INDEX;
                                updateirqs();
                        }
                }
//                cyc=(oldcyc-cycles);
                cycles-=200;
        }
}
#endif
