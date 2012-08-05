#include "rpcemu.h"

#ifdef DYNAREC

/*RPCemu v0.6 by Tom Walker
  SA110 emulation
  Dynamic recompiling version!*/

int blockend;
#define STRONGARM

/*Preliminary FPA emulation. This works to an extent - !Draw works with it, !SICK
  seems to (FPA Whetstone scores are around 100x without), but !AMPlayer doesn't
  work, and GCC stuff tends to crash.*/
//#define FPA

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "rpcemu.h"
#include "hostfs.h"
//#include "codegen_x86.h"
#include "keyboard.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"
#include "cp15.h"
#include "fdc.h"

extern void removeblock(void); /* in codegen_*.c */
	
static int r15diff;
static int fdci=0;
static int mmask;
uint32_t r15mask;
int memmode;
static int cycles;
int prefabort;
uint32_t rotatelookup[4096];
uint32_t inscount;
int armirq=0;
int cpsr;
uint32_t *pcpsr;

uint32_t *usrregs[16];
static uint32_t userregs[17], superregs[17], fiqregs[17], irqregs[17];
static uint32_t abortregs[17], undefregs[17];
static uint32_t spsr[16];
uint32_t armregs[18];
uint32_t mode;
int databort;
int prog32;

#define NFSET ((armregs[cpsr]&0x80000000)?1:0)
#define ZFSET ((armregs[cpsr]&0x40000000)?1:0)
#define CFSET ((armregs[cpsr]&0x20000000)?1:0)
#define VFSET ((armregs[cpsr]&0x10000000)?1:0)

#define GETADDR(r) ((r==15)?(armregs[15]&r15mask):armregs[r])
#define LOADREG(r,v) if (r==15) { armregs[15]=(armregs[15]&~r15mask)|(((v)+4)&r15mask); refillpipeline(); } else armregs[r]=(v);
#define GETREG(r) ((r==15) ? armregs[15]+4 : armregs[r])

#define refillpipeline() blockend=1;

#include "arm_common.h"

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
                fatal("Bad mode %i\n", mode);
        }

        if (ARM_MODE_32(mode)) {
                mmask=31;
                cpsr=16;
                pcpsr=&armregs[16];
                r15mask=0xFFFFFFFC;
                if (!ARM_MODE_32(om)) {
			/* Change from 26-bit to 32-bit mode */
                        armregs[16]=(armregs[15]&0xF0000000)|mode;
                        armregs[16]|=((armregs[15]&0xC000000)>>20);
                        armregs[15]&=0x3FFFFFC;
                }
        }
        else
        {
                mmask=3;
                cpsr=15;
                pcpsr=&armregs[15];
                r15mask=0x3FFFFFC;
                armregs[16]=(armregs[16]&0xFFFFFFE0)|mode;
                if (ARM_MODE_32(om)) {
                        armregs[15]&=r15mask;
                        armregs[15]|=(mode&3);
                        armregs[15]|=(armregs[16]&0xF0000000);
                        armregs[15]|=((armregs[16]&0xC0)<<20);
//                        printf("R15 now %08X\n",armregs[15]);
                }
        }

	/* Update memory access mode based on privilege level of ARM mode */
	memmode = ARM_MODE_PRIV(mode) ? 1 : 0;
}

static int stmlookup[256];

int countbitstable[65536];

void
resetarm(CPUModel cpu_model)
{
        int c,d,exec = 0,data;
//        atexit(dumpregs);
        uint32_t rotval,rotamount;
        resetcodeblocks();

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
                        flaglookup[c][d]=(unsigned char)exec;
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
        pccache=0xFFFFFFFF;
	if (cpu_model == CPUModel_SA110 || cpu_model == CPUModel_ARM810) {
		r15diff = 0;
	} else {
		r15diff = 4;
	}
}

int indumpregs=0;

void dumpregs(void)
{
        char s[1024];

        if (indumpregs) return;
        indumpregs=1;

        sprintf(s, "R 0=%08X R 4=%08X R 8=%08X R12=%08X\n"
                   "R 1=%08X R 5=%08X R 9=%08X R13=%08X\n"
                   "R 2=%08X R 6=%08X R10=%08X R14=%08X\n"
                   "R 3=%08X R 7=%08X R11=%08X R15=%08X\n"
                   "%s\n",
                   armregs[0], armregs[4], armregs[8], armregs[12],
                   armregs[1], armregs[5], armregs[9], armregs[13],
                   armregs[2], armregs[6], armregs[10], armregs[14],
                   armregs[3], armregs[7], armregs[11], armregs[15],
                   mmu ? "MMU enabled" : "MMU disabled");
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
                        armregs[cpsr] |= (cflag << 29);
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

static unsigned
shift5(unsigned opcode, unsigned shiftmode, unsigned shiftamount, uint32_t rm)
{
	switch (shiftmode) {
	case 0: /* LSL */
		if (shiftamount == 0)
			return rm;
		return 0; /* shiftamount >= 32 */

	case 0x20: /* LSR */
		if (shiftamount == 0 && (opcode & 0x10))
			return rm;
		return 0; /* shiftamount >= 32 */

	case 0x40: /* ASR */
		if (shiftamount == 0 && !(opcode & 0x10))
			shiftamount = 32;
		if (shiftamount >= 32) {
			if (rm & 0x80000000)
				return 0xffffffff;
			return 0;
		}
		return (int) rm >> shiftamount;

	default: /* ROR */
		if (!(opcode & 0x10)) {
			/* RRX */
			return (((CFSET) ? 1 : 0) << 31) | (rm >> 1);
		}
		shiftamount &= 0x1f;
		return (rm >> shiftamount) | (rm << (32 - shiftamount));
	}
}

static inline unsigned shift4(unsigned opcode)
{
        unsigned shiftmode=opcode&0x60;
        unsigned shiftamount=(opcode&0x10)?(armregs[(opcode>>8)&15]&0xFF):((opcode>>7)&31);
        uint32_t rm=armregs[RM];
//        if (((opcode>>8)&15)==15 && (opcode&0x10)) rpclog("Shift by R15!!\n");
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

INLINING unsigned shift_ldrstr2(unsigned opcode)
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

#define undefined() exception(UNDEFINED,8,4)

static void bad_opcode(uint32_t opcode) 
{
     error("Bad opcode %02X %08X at %07X\n",(opcode >> 20) & 0xFF, opcode, PC);
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

        if (ARM_MODE_32(mode)) {
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

#define INARMC
#include "ArmDynarecOps.h"

/*void writememl(unsigned long a, unsigned long v)
{
        if (vraddrl[(a)>>12]&3)
           writememfl(a,v);
        else
        {
                *(unsigned long *)((a)+vraddrl[(a)>>12])=v;
        }
}*/

static void badopcode(uint32_t opcode)
{
        bad_opcode(opcode);
        exit(-1);
}

static const unsigned char validforskip[64]=
{
        1,        1,        1,        1,        1,       1,        1,       1,
        1,        1,        1,        1,        1,       1,        1,       1,
        0,        0,        0,        0,        0,       0,        0,       0,
        1,        1,        0,        0,        1,       1,        1,       1,
        
        0,        0,        1,        0,        1,       0,        1,       0,
        1,        0,        1,        0,        1,       0,        1,       0,
        0,        0,        0,        0,        0,       0,        0,       0,
        1,        0,        1,        0,        1,       0,        1,       0
};

static const OpFn opcodes[256]=
{
	opANDreg, opANDregS,opEORreg, opEORregS,opSUBreg,opSUBregS,opRSBreg,opRSBregS,   //00
	opADDreg, opADDregS,opADCreg, opADCregS,opSBCreg,opSBCregS,opRSCreg,opRSCregS,   //08
	opSWPword,opTSTreg, opMSRcreg,opTEQreg, opSWPbyte,opCMPreg,opMSRsreg,opCMNreg,   //10
	opORRreg, opORRregS,opMOVreg, opMOVregS,opBICreg,opBICregS,opMVNreg,opMVNregS,   //18

	opANDimm, opANDimmS,opEORimm, opEORimmS,opSUBimm, opSUBimmS,opRSBimm, opRSBimmS, //20
	opADDimm, opADDimmS,opADCimm, opADCimmS,opSBCimm, opSBCimmS,opRSCimm, opRSCimmS, //28
	badopcode,opTSTimm, opMSRcimm,opTEQimm, badopcode,opCMPimm, badopcode,opCMNimm,  //30
	opORRimm, opORRimmS,opMOVimm, opMOVimmS,opBICimm, opBICimmS,opMVNimm, opMVNimmS, //38

	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRT,   (OpFn)opLDRT,   (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRBT,  (OpFn)opLDRBT,   //40
	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRT,   (OpFn)opLDRT,   (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRBT,  (OpFn)opLDRBT,   //48
	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRB,   (OpFn)opLDRB,    //50
	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRB,   (OpFn)opLDRB,    //58

	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRT,   (OpFn)opLDRT,   (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRBT,  (OpFn)opLDRBT,   //60
	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRT,   (OpFn)opLDRT,   (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRBT,  (OpFn)opLDRBT,   //68
	(OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRB,   (OpFn)opLDRB,    //70
        (OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTR,    (OpFn)opLDR,    (OpFn)opSTRB,   (OpFn)opLDRB,   (OpFn)opSTRB,   (OpFn)opLDRB,    //78

	(OpFn)opSTMD,   (OpFn)opLDMD,   (OpFn)opSTMD,   (OpFn)opLDMD,   (OpFn)opSTMDS,  (OpFn)opLDMDS,  (OpFn)opSTMDS,  (OpFn)opLDMDS,   //80
	(OpFn)opSTMI,   (OpFn)opLDMI,   (OpFn)opSTMI,   (OpFn)opLDMI,   (OpFn)opSTMIS,  (OpFn)opLDMIS,  (OpFn)opSTMIS,  (OpFn)opLDMIS,   //88
	(OpFn)opSTMD,   (OpFn)opLDMD,   (OpFn)opSTMD,   (OpFn)opLDMD,   (OpFn)opSTMDS,  (OpFn)opLDMDS,  (OpFn)opSTMDS,  (OpFn)opLDMDS,   //90
	(OpFn)opSTMI,   (OpFn)opLDMI,   (OpFn)opSTMI,   (OpFn)opLDMI,   (OpFn)opSTMIS,  (OpFn)opLDMIS,  (OpFn)opSTMIS,  (OpFn)opLDMIS,   //98

	opB,	  opB,	    opB,      opB,      opB,      opB,      opB,      opB,       //A0
	opB,	  opB,	    opB,      opB,      opB,      opB,      opB,      opB,       //A8
	opBL,	  opBL,	    opBL,     opBL,     opBL,     opBL,     opBL,     opBL,      //B0
	opBL,	  opBL,	    opBL,     opBL,     opBL,     opBL,     opBL,     opBL,      //B8

	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   //C0
	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   //C8
	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   //D0
	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   //D8

	opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,     //E0
	opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,     //E8
	opSWI,	  opSWI,    opSWI,    opSWI,	opSWI,	  opSWI,    opSWI,    opSWI,     //F0
	opSWI,	  opSWI,    opSWI,    opSWI,	opSWI,	  opSWI,    opSWI,    opSWI      //F8
};

int linecyc=0;
#ifdef __amd64__
#include "codegen_amd64.h" 
#else
        #if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32
        #include "codegen_x86.h"
        #else
                #error Fatal error : no recompiler available for your architecture
        #endif
#endif

void execarm(int cycs)
{
        //int target;
        int hash;
        void (*gen_func)(void);
	uint32_t opcode;
        uint32_t templ;//,templ2,addr,addr2,mask;
        //unsigned char temp;
//        int RD;
        cycles+=cycs;
        linecyc=256;
        while (cycles>0)
        {
//                cyccount+=200;
//                linecyc=200;
//                while (linecyc>0)
//                for (linecyc=0;linecyc<200;linecyc++)
                while (linecyc>=0)
                {
                        armirq&=~0xC0;
                        if (!isblockvalid(PC)) /*Interpret block*/
                        {
                                blockend=0;
                                if ((PC>>12)!=pccache)
                                {
                                        pccache=PC>>12;
                                        pccache2=getpccache(PC);
                                        if (pccache2==NULL) { opcode=pccache=0xFFFFFFFF; armirq|=0x80; }
                                        else                  opcode=pccache2[PC>>2];
                                }
                                while (!blockend && !(armirq&0xC0))
                                {
                                        opcode=pccache2[PC>>2];
                                        if ((opcode & 0x0E000000) == 0x0A000000) { blockend = 1; } /* Always end block on branches */
                                        if ((opcode & 0x0F000000) == 0x0F000000) { blockend = 1; } /* And SWIs and copro stuff */
                                        if (!(opcode & 0xC000000) && (RD == 15)) { blockend = 1; } /* End if R15 can be modified */
                                        if ((opcode & 0x0E108000) == 0x08108000) { blockend = 1; } /* End if R15 reloaded from LDM */
                                        if ((opcode & 0x0C100000) == 0x04100000 && (RD==15)) { blockend = 1; } /* End if R15 reloaded from LDR */
                                        if (flaglookup[opcode>>28][(*pcpsr)>>28])// && !(armirq&0x80))
                                           opcodes[(opcode>>20)&0xFF](opcode);
//                                                if ((opcode&0x0E000000)==0x0A000000) blockend=1; /*Always end block on branches*/
//                                                if ((opcode&0x0C000000)==0x0C000000) blockend=1; /*And SWIs and copro stuff*/
                                        armregs[15]+=4;
                                        if (!((PC)&0xFFC)) blockend=1;
//                                        if (armirq) blockend=1;
                                        inscount++;
//                                        linecyc++;
                                }
                        linecyc--;
//                                linecyc+=
                        }
                        else
                        {
                                hash=HASH(PC);
/*                                if (pagedirty[PC>>9])
                                {
                                        pagedirty[PC>>9]=0;
                                        cacheclearpage(PC>>9);
                                }
                                else */if (codeblockpc[hash]==PC)
                                {
                                        templ=codeblocknum[hash];
                                        gen_func=(void *)(&rcodeblock[templ][BLOCKSTART]);
//                                        gen_func=(void *)(&codeblock[blocks[templ]>>24][blocks[templ]&0xFFF][4]);
                                        gen_func();
                                        if (armirq&0x40) armregs[15]+=4;
                                        if ((armregs[cpsr]&mmask)!=mode) updatemode(armregs[cpsr]&mmask);
                                }
                                else
                                {
                                        blockend=0;
                                        /* Initialise 'opcode' to invalid value */
                                        opcode = 0xffffffff;
                                        if ((PC>>12)!=pccache)
                                        {
                                                pccache=PC>>12;
                                                pccache2=getpccache(PC);
                                                if (pccache2 == NULL) {
                                                        pccache = 0xffffffff;
                                                        armirq |= 0x80;
                                                } else {
                                                        opcode = pccache2[PC >> 2];
                                                }
                                        }
                                        if (!(armirq&0x80)) 
					{
						initcodeblock(PC);
						//printf("New block %08X %04X %08X\n",PC,hash,codeblockpc[hash]);
//codeblockpc[hash]=PC;
}
                                        while (!blockend && !(armirq&0xC0))
                                        {
                                                opcode=pccache2[PC>>2];
                                                if ((opcode>>28)==0xF) /*NV*/
                                                {
                                                        generatepcinc();
                                                }
                                                else
                                                {
                                                        #ifdef ABORTCHECKING
                                                        generateupdatepc();
                                                        #else
                                                        if ((opcode&0xE000000)==0x0000000/* && (RN==15 || RD==15 || RM==15 || !validforskip[(opcode>>20)&63])*/) generateupdatepc();
                                                        if ((opcode&0xE000000)==0x2000000/* && (RN==15 || RD==15 ||           !validforskip[(opcode>>20)&63])*/) generateupdatepc();
                                                        if ((opcode&0xC000000)==0x4000000 && (RN==15 || RD==15 || RM==15)) generateupdatepc();
                                                        if ((opcode&0xE000000)==0x8000000 && ((opcode&0x8000) || (RN==15))) generateupdatepc();
                                                        if ((opcode&0xF000000)>=0xA000000) generateupdatepc();
                                                        #endif
//                                                        if (((opcode+0x6000000)&0xF000000)>=0xA000000) generateupdatepc();
//                                                        generateupdatepc();
                                                        generatepcinc();
                                                        if ((opcode&0xE000000)==0xA000000) generateupdateinscount();
                                                        if ((opcode>>28)!=0xE) generateflagtestandbranch(opcode,pcpsr);//,flaglookup);
                                                        else                   lastflagchange=0;
                                                        generatecall(opcodes[(opcode>>20)&0xFF],opcode,pcpsr);
                                                        #ifdef ABORTCHECKING
                                                        if (((opcode+0x6000000)&0xF000000)>=0xA000000) generateirqtest();
                                                        #endif
//                                                        if ((opcode&0x0E000000)==0x0A000000) blockend=1; /*Always end block on branches*/
                                                        if ((opcode&0x0C000000)==0x0C000000) blockend=1; /*And SWIs and copro stuff*/
                                                        if (!(opcode&0xC000000) && (RD==15)) blockend=1; /*End if R15 can be modified*/
                                                        if ((opcode&0x0E108000)==0x08108000) blockend=1; /*End if R15 reloaded from LDM*/
                                                        if ((opcode&0x0C100000)==0x04100000 && (RD==15)) blockend=1; /*End if R15 reloaded from LDR*/
                                                        if (flaglookup[opcode>>28][(*pcpsr)>>28])// && !(armirq&0x80))
                                                           opcodes[(opcode>>20)&0xFF](opcode);
                                                }
                                                armregs[15]+=4;
                                                if (!((PC)&0xFFC))
                                                {
                                                        blockend=1;
/*                                                        pccache=PC>>12;
                                                        pccache2=getpccache(PC);
                                                        if (pccache2==NULL) { opcode=pccache=0xFFFFFFFF; armirq|=0x80; blockend=1; rpclog("Abort!\n"); }
                                                        else                  opcode=pccache2[PC>>2];*/
                                                }
                                                //blockend=1;
//                                                inscount++;
                                        }
                                        if (!(armirq & 0x80)) {
                                                endblock(opcode, pcpsr);
                                        } else {
                                                removeblock();
                                        }
                                }
                        linecyc--;
                        }

//                        linecyc+=10;
                        if (/*databort|*/armirq&0xC3)//|prefabort)
                        {
                                if (!ARM_MODE_32(mode)) {
                                        armregs[16]&=~0xC0;
                                        armregs[16]|=((armregs[15]&0xC000000)>>20);
                                }

                                if (armirq&0xC0)
                                {
//                                        if (armirq&0xC0) rpclog("Will abort\n");
//                                        exception(ABORT,(armirq&0x80)?0x10:0x14,0);
//                                        armirq&=~0xC0;
//                                        #if 0
                                        if (armirq&0x80)//prefabort)       /*Prefetch abort*/
                                        {
                                                armregs[15]-=4;
                                                exception(ABORT, 0x10, 4);
                                                armregs[15]+=4;
                                                armirq&=~0xC0;
                                        }
                                        else if (armirq&0x40)//databort==1)     /*Data abort*/
                                        {
                                                armregs[15]-=4;
                                                exception(ABORT, 0x14, 0);
                                                armregs[15]+=4;
                                                armirq&=~0xC0;
                                        }
                                }
                                else if ((armirq&2) && !(armregs[16]&0x40)) /*FIQ*/
                                {
                                        armregs[15]-=4;
                                        exception(FIQ,0x20,0);
                                        armregs[15]+=4;
                                }
                                else if ((armirq&1) && !(armregs[16]&0x80)) /*IRQ*/
                                {
                                        armregs[15]-=4;
                                        exception(IRQ, 0x1c, 0);
                                        armregs[15]+=4;
                                }
                        }
//                        armirq=(armirq&0xCC)|((armirq>>2)&3);
                }
                linecyc+=256;

                if (kcallback)
                {
                        kcallback--;
                        if (kcallback<=0)
                        {
                                kcallback=0;
                                keyboard_callback_rpcemu();
                        }
                }
                if (mcallback)
                {
                        mcallback-=10;
                        if (mcallback<=0)
                        {
                                mcallback=0;
                                mouse_ps2_callback();
                        }
                }
                if (fdccallback)
                {
                        fdccallback-=50;
                        if (fdccallback<=0)
                        {
                                fdccallback=0;
                                fdc_callback();
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
                cycles-=1000;
        }
}
#endif
