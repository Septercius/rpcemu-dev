/*RPCemu v0.6 by Tom Walker
  ARM6/7 emulation*/

#include "rpcemu.h"

#ifndef DYNAREC

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

#include "hostfs.h"
#include "arm.h"
#include "cp15.h"
#include "mem.h"
#include "iomd.h"
#include "keyboard.h"
#include "fdc.h"
#include "ide.h"

ARMState arm;

int blockend;
static int fdci=0;
uint32_t r15mask;
static int cycles;
int prefabort;
uint32_t rotatelookup[4096];
uint32_t inscount;
int armirq=0;
int cpsr;
static uint32_t *pcpsr;

static uint8_t flaglookup[16][16];

uint32_t *usrregs[16];
int databort;
int prog32;

#define NFSET ((arm.reg[cpsr] & NFLAG) ? 1 : 0)
#define ZFSET ((arm.reg[cpsr] & ZFLAG) ? 1 : 0)
#define CFSET ((arm.reg[cpsr] & CFLAG) ? 1 : 0)
#define VFSET ((arm.reg[cpsr] & VFLAG) ? 1 : 0)

#define GETADDR(r) ((r == 15) ? (arm.reg[15] & r15mask) : arm.reg[r])
#define LOADREG(r,v) if (r==15) { arm.reg[15]=(arm.reg[15]&~r15mask)|(((v)+4)&r15mask); refillpipeline(); } else arm.reg[r]=(v);
#define GETREG(r) ((r == 15) ? (arm.reg[15] + arm.r15_diff) : arm.reg[r])

#define refillpipeline()

#include "arm_common.h"

uint32_t pccache,*pccache2;

void updatemode(uint32_t m)
{
        uint32_t c, om = arm.mode;

        usrregs[15] = &arm.reg[15];
        switch (arm.mode & 0xf) { /* Store back registers */
            case USER:
            case SYSTEM: /* System (ARMv4) shares same bank as User mode */
                for (c=8;c<15;c++) arm.user_reg[c] = arm.reg[c];
                break;

            case IRQ:
                for (c=8;c<13;c++) arm.user_reg[c] = arm.reg[c];
                arm.irq_reg[0] = arm.reg[13];
                arm.irq_reg[1] = arm.reg[14];
                break;

            case FIQ:
                for (c=8;c<15;c++) arm.fiq_reg[c] = arm.reg[c];
                break;

            case SUPERVISOR:
                for (c=8;c<13;c++) arm.user_reg[c] = arm.reg[c];
                arm.super_reg[0] = arm.reg[13];
                arm.super_reg[1] = arm.reg[14];
                break;

            case ABORT:
                for (c=8;c<13;c++) arm.user_reg[c] = arm.reg[c];
                arm.abort_reg[0] = arm.reg[13];
                arm.abort_reg[1] = arm.reg[14];
                break;

            case UNDEFINED:
                for (c=8;c<13;c++) arm.user_reg[c] = arm.reg[c];
                arm.undef_reg[0] = arm.reg[13];
                arm.undef_reg[1] = arm.reg[14];
                break;
        }
        arm.mode = m;

        switch (m&15)
        {
            case USER:
            case SYSTEM:
                for (c=8;c<15;c++) arm.reg[c] = arm.user_reg[c];
                for (c=0;c<15;c++) usrregs[c] = &arm.reg[c];
                break;

            case IRQ:
                for (c=8;c<13;c++) arm.reg[c] = arm.user_reg[c];
                arm.reg[13] = arm.irq_reg[0];
                arm.reg[14] = arm.irq_reg[1];
                for (c=0;c<13;c++) usrregs[c] = &arm.reg[c];
                for (c=13;c<15;c++) usrregs[c] = &arm.user_reg[c];
                break;
            
            case FIQ:
                for (c=8;c<15;c++) arm.reg[c] = arm.fiq_reg[c];
                for (c=0;c<8;c++)  usrregs[c] = &arm.reg[c];
                for (c=8;c<15;c++) usrregs[c] = &arm.user_reg[c];
                break;

            case SUPERVISOR:
                for (c=8;c<13;c++) arm.reg[c] = arm.user_reg[c];
                arm.reg[13] = arm.super_reg[0];
                arm.reg[14] = arm.super_reg[1];
                for (c=0;c<13;c++) usrregs[c] = &arm.reg[c];
                for (c=13;c<15;c++) usrregs[c] = &arm.user_reg[c];
                break;
            
            case ABORT:
                for (c=8;c<13;c++) arm.reg[c] = arm.user_reg[c];
                arm.reg[13] = arm.abort_reg[0];
                arm.reg[14] = arm.abort_reg[1];
                for (c=0;c<13;c++) usrregs[c] = &arm.reg[c];
                for (c=13;c<15;c++) usrregs[c] = &arm.user_reg[c];
                break;

            case UNDEFINED:
                for (c=8;c<13;c++) arm.reg[c] = arm.user_reg[c];
                arm.reg[13] = arm.undef_reg[0];
                arm.reg[14] = arm.undef_reg[1];
                for (c=0;c<13;c++) usrregs[c] = &arm.reg[c];
                for (c=13;c<15;c++) usrregs[c] = &arm.user_reg[c];
                break;

            default:
                fatal("Bad mode %i\n", arm.mode);
        }

        if (ARM_MODE_32(arm.mode)) {
                arm.mmask = 0x1f;
                cpsr=16;
                pcpsr = &arm.reg[16];
                r15mask=0xFFFFFFFC;
                if (!ARM_MODE_32(om)) {
			/* Change from 26-bit to 32-bit mode */
                        arm.reg[16] = (arm.reg[15] & 0xf0000000) | arm.mode;
                        arm.reg[16] |= ((arm.reg[15] & 0xc000000) >> 20);
                        arm.reg[15] &= 0x3fffffc;
                }
        }
        else
        {
                arm.mmask = 3;
                cpsr=15;
                pcpsr = &arm.reg[15];
                r15mask=0x3FFFFFC;
                arm.reg[16] = (arm.reg[16] & 0xffffffe0) | arm.mode;
                if (ARM_MODE_32(om)) {
                        arm.reg[15] &= r15mask;
                        arm.reg[15] |= (arm.mode & 3);
                        arm.reg[15] |= (arm.reg[16] & 0xf0000000);
                        arm.reg[15] |= ((arm.reg[16] & 0xc0) << 20);
                }
        }

	/* Update memory access mode based on privilege level of ARM mode */
	memmode = ARM_MODE_PRIV(arm.mode) ? 1 : 0;
}

static int stmlookup[256];

int countbitstable[65536];

void
arm_init(void)
{
	unsigned c, d, exec, data;

	for (c = 0; c < 256; c++) {
		stmlookup[c] = 0;
		for (d = 0; d < 8; d++) {
			if (c & (1u << d)) {
				stmlookup[c] += 4;
			}
		}
	}
	for (c = 0; c < 65536; c++) {
		countbitstable[c] = 0;
		for (d = 0; d < 16; d++) {
			if (c & (1u << d)) {
				countbitstable[c] += 4;
			}
		}
	}

	cpsr = 15;
	for (c = 0; c < 16; c++) {
		for (d = 0; d < 16; d++) {
			arm.reg[15] = d << 28;
			switch (c) {
			case 0:  /* EQ */ exec = ZFSET; break;
			case 1:  /* NE */ exec = !ZFSET; break;
			case 2:  /* CS */ exec = CFSET; break;
			case 3:  /* CC */ exec = !CFSET; break;
			case 4:  /* MI */ exec = NFSET; break;
			case 5:  /* PL */ exec = !NFSET; break;
			case 6:  /* VS */ exec = VFSET; break;
			case 7:  /* VC */ exec = !VFSET; break;
			case 8:  /* HI */ exec = (CFSET && !ZFSET); break;
			case 9:  /* LS */ exec = (!CFSET || ZFSET); break;
			case 10: /* GE */ exec = (NFSET == VFSET); break;
			case 11: /* LT */ exec = (NFSET != VFSET); break;
			case 12: /* GT */ exec = (!ZFSET && (NFSET == VFSET)); break;
			case 13: /* LE */ exec = (ZFSET || (NFSET != VFSET)); break;
			case 14: /* AL */ exec = 1; break;
			case 15: /* NV */ exec = 0; break;
			}
			flaglookup[c][d] = (uint8_t) exec;
		}
	}

	for (data = 0; data < 4096; data++) {
		uint32_t val = data & 0xff;
		uint32_t amount = ((data >> 8) & 0xf) << 1;

		rotatelookup[data] = (val >> amount) | (val << (32 - amount));
	}
}

void
resetarm(CPUModel cpu_model)
{
//        atexit(dumpregs);

	memset(&arm, 0, sizeof(arm));

        r15mask=0x3FFFFFC;
        pccache=0xFFFFFFFF;
        updatemode(SUPERVISOR);
        cpsr=15;
//        prog32=1;

        arm.reg[15] = 0x0c000008 | 3;
        arm.reg[16] = SUPERVISOR | 0xd0;
        arm.mode = SUPERVISOR;
        pccache=0xFFFFFFFF;
	if (cpu_model == CPUModel_SA110 || cpu_model == CPUModel_ARM810) {
		arm.r15_diff = 0;
		arm.abort_base_restored = 1;
		arm.stm_writeback_at_end = 1;
	} else {
		arm.r15_diff = 4;
		arm.abort_base_restored = 0;
		arm.stm_writeback_at_end = 0;
	}

	cycles = 0;
}

void dumpregs(void)
{
        char s[1024];

        sprintf(s, "R 0=%08X R 4=%08X R 8=%08X R12=%08X\n"
                   "R 1=%08X R 5=%08X R 9=%08X R13=%08X\n"
                   "R 2=%08X R 6=%08X R10=%08X R14=%08X\n"
                   "R 3=%08X R 7=%08X R11=%08X R15=%08X\n"
                   "%s\n",
                   arm.reg[0], arm.reg[4], arm.reg[8], arm.reg[12],
                   arm.reg[1], arm.reg[5], arm.reg[9], arm.reg[13],
                   arm.reg[2], arm.reg[6], arm.reg[10], arm.reg[14],
                   arm.reg[3], arm.reg[7], arm.reg[11], arm.reg[15],
                   mmu ? "MMU enabled" : "MMU disabled");
        rpclog("%s",s);
        printf("%s",s);

        memmode=1;
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
                shiftamount = arm.reg[(opcode >> 8) & 0xf] & 0xff;
        }
        temp = arm.reg[RM];
        if (shiftamount) arm.reg[cpsr] &= ~CFLAG;
        switch (shiftmode)
        {
                case 0: /*LSL*/
                if (!shiftamount) return temp;
                if (shiftamount==32)
                {
                        if (temp&1) arm.reg[cpsr] |= CFLAG;
                        return 0;
                }
                if (shiftamount>32) return 0;
                if ((temp<<(shiftamount-1))&0x80000000) arm.reg[cpsr] |= CFLAG;
                return temp<<shiftamount;

                case 0x20: /*LSR*/
                if (!shiftamount && !(opcode&0x10))
                {
                        shiftamount=32;
                }
                if (!shiftamount) return temp;
                if (shiftamount==32)
                {
                        if (temp&0x80000000) arm.reg[cpsr] |= CFLAG;
                        else                 arm.reg[cpsr] &= ~CFLAG;
                        return 0;
                }
                if (shiftamount>32) return 0;
                if ((temp>>(shiftamount-1))&1) arm.reg[cpsr] |= CFLAG;
                return temp>>shiftamount;

                case 0x40: /*ASR*/
                if (!shiftamount)
                {
                        if (opcode&0x10) return temp;
                }
                if (shiftamount>=32 || !shiftamount)
                {
                        if (temp&0x80000000) arm.reg[cpsr] |= CFLAG;
                        else                 arm.reg[cpsr] &= ~CFLAG;
                        if (temp&0x80000000) return 0xFFFFFFFF;
                        return 0;
                }
                if (((int)temp>>(shiftamount-1))&1) arm.reg[cpsr] |= CFLAG;
                return (int)temp>>shiftamount;

                default: /*ROR*/
                arm.reg[cpsr] &= ~CFLAG;
                if (!shiftamount && !(opcode&0x10))
                {
                        if (temp&1) arm.reg[cpsr] |= CFLAG;
                        return (((cflag)?1:0)<<31)|(temp>>1);
                }
                if (!shiftamount)
                {
                        arm.reg[cpsr] |= (cflag << 29);
                        return temp;
                }
                if (!(shiftamount&0x1F))
                {
                        if (temp&0x80000000) arm.reg[cpsr] |= CFLAG;
                        return temp;
                }
                if (((temp>>shiftamount)|(temp<<(32-shiftamount)))&0x80000000) arm.reg[cpsr] |= CFLAG;
                return (temp>>shiftamount)|(temp<<(32-shiftamount));
                break;
        }
}

#define shift(o)  ((o & 0xff0) ? shift3(o) : arm.reg[RM])
#define shift2(o) ((o & 0xff0) ? shift4(o) : arm.reg[RM])
#define shift_ldrstr(o) shift2(o)

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
        unsigned shiftamount=(opcode&0x10)?(arm.reg[(opcode>>8)&15]&0xFF):((opcode>>7)&31);
        uint32_t rm = arm.reg[RM];

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

static inline unsigned rotate(unsigned data)
{
        uint32_t rotval;
        rotval=rotatelookup[data&4095];
        if (/*data&0x100000 && */data&0xF00)
        {
                if (rotval&0x80000000) arm.reg[cpsr] |= CFLAG;
                else                   arm.reg[cpsr] &= ~CFLAG;
        }
        return rotval;
}

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

        if (ARM_MODE_32(arm.mode)) {
                templ = arm.reg[15] - diff;
                arm.spsr[mmode] = arm.reg[16];
                updatemode(mmode|16);
                arm.reg[14] = templ;
                arm.reg[16] &= ~0x1f;
                arm.reg[16] |= 0x10 | mmode | irq_disable;
                arm.reg[15] = address;
                refillpipeline();
        }
        else if (prog32)
        {
                templ = arm.reg[15] - diff;
                updatemode(mmode|16);
                arm.reg[14] = templ & 0x3fffffc;
                arm.spsr[mmode] = (arm.reg[16] & ~0x1f) | (templ & 3);
                arm.spsr[mmode] &= ~0x10;
                arm.reg[16] |= irq_disable;
                arm.reg[15] = address;
                refillpipeline();
        }
        else
        {
                templ = arm.reg[15] - diff;
                arm.reg[15] |= 3;
                /* When in 26-bit config, Abort and Undefined exceptions enter
                   mode SVC_26 */
                updatemode(mmode >= SUPERVISOR ? SUPERVISOR : mmode);
                arm.reg[14] = templ;
                arm.reg[15] &= 0xfc000003;
                arm.reg[15] |= ((irq_disable << 20) | address);
                refillpipeline();
        }
}

void execarm(int cycs)
{
        int linecyc;
	uint32_t opcode;
	uint32_t lhs, rhs, dest;
	uint32_t templ, templ2, addr, addr2, offset, writeback;

        cycles+=cycs;
        while (cycles>0)
        {
//                cyccount+=200;
//                linecyc=200;
//                while (linecyc>0)
                for (linecyc=0;linecyc<200;linecyc++)
                {
                        if ((PC>>12)!=pccache)
                        {
                                pccache=PC>>12;
                                pccache2=getpccache(PC);
                                if (pccache2==NULL) opcode=pccache=0xFFFFFFFF;
                                else                      opcode=pccache2[PC>>2];
                        }
                        else
                           opcode=pccache2[PC>>2];

                        if (flaglookup[opcode>>28][(*pcpsr)>>28] && !(armirq&0x80))//prefabort)
                        {
#ifdef STRONGARM
//                                if ((opcode&0xE000090)==0x90)
//                                {
                                if ((opcode&0xE0000F0)==0xB0) /*LDRH/STRH*/
                                {
                                        fatal("Bad LDRH/STRH opcode %08X\n", opcode);
                                }
                                else if ((opcode&0xE1000D0)==0x1000D0) /*LDRS*/
                                {
                                        fatal("Bad LDRH/STRH opcode %08X\n", opcode);
//                                }
//                                goto domain;
                                }
                                else
                                {
//                                        domain:
//                                        GETRD;
#endif
                                switch ((opcode >> 20) & 0xff) {
				case 0x00: /* AND reg */
					if ((opcode & 0xf0) == 0x90) /* MUL */
					{
						arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
						    (arm.reg[MULRM] * arm.reg[MULRS]);
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
						arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
						    (arm.reg[MULRM] * arm.reg[MULRS]);
						setzn(arm.reg[MULRD]);
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
						       dest = lhs & shift(opcode);
						       arm.reg[RD] = dest;
						       setzn(dest);
					       }
					}
					break;

				case 0x02: /* EOR reg */
					if ((opcode & 0xf0) == 0x90) /* MLA */
					{
						arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
						    (arm.reg[MULRM] * arm.reg[MULRS]) + arm.reg[MULRN];
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
						arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
						    (arm.reg[MULRM] * arm.reg[MULRS]) + arm.reg[MULRN];
						setzn(arm.reg[MULRD]);
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
                                                        dest = lhs ^ shift(opcode);
                                                        arm.reg[RD] = dest;
                                                        setzn(dest);
                                                }
                                        }
                                        break;

                                case 0x04: /* SUB reg */
                                        dest = GETADDR(RN) - shift2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x05: /* SUBS reg */
                                        lhs = GETADDR(RN);
                                        rhs = shift2(opcode);
                                        dest = lhs - rhs;
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setsub(lhs, rhs, dest);
                                                arm.reg[RD] = dest;
                                        }
                                        break;

                                case 0x06: /* RSB reg */
                                        dest = shift2(opcode) - GETADDR(RN);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x07: /* RSBS reg */
                                        lhs = GETADDR(RN);
                                        rhs = shift2(opcode);
                                        dest = rhs - lhs;
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setsub(rhs, lhs, dest);
                                                arm.reg[RD] = dest;
                                        }
                                        break;

                                case 0x08: /* ADD reg */
#ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* UMULL */
					{
                                                uint64_t mula = (uint64_t) arm.reg[MULRS];
                                                uint64_t mulb = (uint64_t) arm.reg[MULRM];
                                                uint64_t mulres = mula * mulb;

                                                arm.reg[MULRN] = (uint32_t) mulres;
                                                arm.reg[MULRD] = (uint32_t) (mulres >> 32);
                                                break;
                                        }
#endif
                                        dest = GETADDR(RN) + shift2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x09: /* ADDS reg */
#ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* UMULLS */
					{
                                                uint64_t mula = (uint64_t) arm.reg[MULRS];
                                                uint64_t mulb = (uint64_t) arm.reg[MULRM];
                                                uint64_t mulres = mula * mulb;

                                                arm.reg[MULRN] = (uint32_t) mulres;
                                                arm.reg[MULRD] = (uint32_t) (mulres >> 32);
                                                arm_flags_long_multiply(mulres);
                                                break;
                                        }
#endif
                                        lhs = GETADDR(RN);
                                        rhs = shift2(opcode);
                                        dest = lhs + rhs;
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setadd(lhs, rhs, dest);
                                                arm.reg[RD] = dest;
                                        }
                                        break;
                                
                                case 0x0A: /* ADC reg */
#ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* UMLAL */
					{
                                                uint64_t mula = (uint64_t) arm.reg[MULRS];
                                                uint64_t mulb = (uint64_t) arm.reg[MULRM];
                                                uint64_t current = ((uint64_t) arm.reg[MULRD] << 32) |
                                                                   arm.reg[MULRN];
                                                uint64_t mulres = (mula * mulb) + current;

                                                arm.reg[MULRN] = (uint32_t) mulres;
                                                arm.reg[MULRD] = (uint32_t) (mulres >> 32);
                                                break;
                                        }
#endif
                                        dest = GETADDR(RN) + shift2(opcode) + CFSET;
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x0B: /* ADCS reg */
#ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* UMLALS */
					{
                                                uint64_t mula = (uint64_t) arm.reg[MULRS];
                                                uint64_t mulb = (uint64_t) arm.reg[MULRM];
                                                uint64_t current = ((uint64_t) arm.reg[MULRD] << 32) |
                                                                   arm.reg[MULRN];
                                                uint64_t mulres = (mula * mulb) + current;

                                                arm.reg[MULRN] = (uint32_t) mulres;
                                                arm.reg[MULRD] = (uint32_t) (mulres >> 32);
                                                arm_flags_long_multiply(mulres);
                                                break;
                                        }
#endif
                                        lhs = GETADDR(RN);
                                        rhs = shift2(opcode);
                                        dest = lhs + rhs + CFSET;
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setadc(lhs, rhs, dest);
                                                arm.reg[RD] = dest;
                                        }
                                        break;

                                case 0x0C: /* SBC reg */
#ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* SMULL */
					{
                                                int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
                                                int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
                                                int64_t mulres = mula * mulb;

                                                arm.reg[MULRN] = (uint32_t) mulres;
                                                arm.reg[MULRD] = (uint32_t) (mulres >> 32);
                                                break;
                                        }
#endif
                                        dest = GETADDR(RN) - shift2(opcode) - ((CFSET) ? 0 : 1);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x0D: /* SBCS reg */
#ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* SMULLS */
					{
                                                int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
                                                int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
                                                int64_t mulres = mula * mulb;

                                                arm.reg[MULRN] = (uint32_t) mulres;
                                                arm.reg[MULRD] = (uint32_t) (mulres >> 32);
                                                arm_flags_long_multiply(mulres);
                                                break;
                                        }
#endif
                                        lhs = GETADDR(RN);
                                        rhs = shift2(opcode);
                                        dest = lhs - rhs - (CFSET ? 0 : 1);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setsbc(lhs, rhs, dest);
                                                arm.reg[RD] = dest;
                                        }
                                        break;

                                case 0x0E: /* RSC reg */
#ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* SMLAL */
					{
                                                int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
                                                int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
                                                int64_t current = ((int64_t) arm.reg[MULRD] << 32) |
                                                                   arm.reg[MULRN];
                                                int64_t mulres = (mula * mulb) + current;

                                                arm.reg[MULRN] = (uint32_t) mulres;
                                                arm.reg[MULRD] = (uint32_t) (mulres >> 32);
                                                break;
                                        }
#endif
                                        dest = shift2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x0F: /* RSCS reg */
#ifdef STRONGARM
					if ((opcode & 0xf0) == 0x90) /* SMLALS */
					{
                                                int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
                                                int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
                                                int64_t current = ((int64_t) arm.reg[MULRD] << 32) |
                                                                   arm.reg[MULRN];
                                                int64_t mulres = (mula * mulb) + current;

                                                arm.reg[MULRN] = (uint32_t) mulres;
                                                arm.reg[MULRD] = (uint32_t) (mulres >> 32);
                                                arm_flags_long_multiply(mulres);
                                                break;
                                        }
#endif
                                        lhs = GETADDR(RN);
                                        rhs = shift2(opcode);
                                        dest = rhs - lhs - (CFSET ? 0 : 1);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setsbc(rhs, lhs, dest);
                                                arm.reg[RD] = dest;
                                        }
                                        break;

				case 0x10: /* MRS reg,CPSR and SWP */
					if ((opcode & 0xf0) == 0x90) {
						/* SWP */
						if (RD != 15) {
							addr = GETADDR(RN);
							templ = GETREG(RM);
							dest = readmeml(addr);
							if (armirq & 0x40) {
								break;
							}
							dest = arm_ldr_rotate(dest, addr);
							writememl(addr, templ);
							if (armirq & 0x40) {
								break;
							}
							LOADREG(RD, dest);
						}
                                        }
                                        else if (!(opcode&0xFFF)) /*MRS CPSR*/
                                        {
                                                if (!ARM_MODE_32(arm.mode)) {
                                                        arm.reg[16] = (arm.reg[15] & 0xf0000000) | (arm.reg[15] & 3);
                                                        arm.reg[16] |= ((arm.reg[15] & 0xc000000) >> 20);
                                                }
                                                arm.reg[RD] = arm.reg[16];
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
						arm_write_cpsr(opcode, arm.reg[RM]);
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
					if ((opcode & 0xf0) == 0x90) {
						/* SWPB */
						if (RD != 15) {
							addr = GETADDR(RN);
							templ = GETREG(RM);
							dest = readmemb(addr);
							if (armirq & 0x40) {
								break;
							}
							writememb(addr, templ);
							if (armirq & 0x40) {
								break;
							}
							LOADREG(RD, dest);
						}
                                        } else if (!(opcode&0xFFF)) /* MRS SPSR */
                                        {
                                                arm.reg[RD] = arm.spsr[arm.mode & 0xf];
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
						arm_write_spsr(opcode, arm.reg[RM]);
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
                                                dest = lhs | shift(opcode);
                                                arm.reg[RD] = dest;
                                                setzn(dest);
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
                                                arm.reg[RD] = shift(opcode);
                                                setzn(arm.reg[RD]);
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
                                                dest = lhs & ~shift(opcode);
                                                arm.reg[RD] = dest;
                                                setzn(dest);
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
                                                arm.reg[RD] = ~shift(opcode);
                                                setzn(arm.reg[RD]);
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
                                                dest = lhs & rotate(opcode);
                                                arm.reg[RD] = dest;
                                                setzn(dest);
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
                                                dest = lhs ^ rotate(opcode);
                                                arm.reg[RD] = dest;
                                                setzn(dest);
                                        }
                                        break;

                                case 0x24: /* SUB imm */
                                        dest = GETADDR(RN) - rotate2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x25: /* SUBS imm */
                                        lhs = GETADDR(RN);
                                        rhs = rotate2(opcode);
                                        dest = lhs - rhs;
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                arm.reg[RD] = dest;
                                                setsub(lhs, rhs, dest);
                                        }
                                        break;

                                case 0x26: /* RSB imm */
                                        dest = rotate2(opcode) - GETADDR(RN);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x27: /* RSBS imm */
                                        lhs = GETADDR(RN);
                                        rhs = rotate2(opcode);
                                        dest = rhs - lhs;
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setsub(rhs, lhs, dest);
                                                arm.reg[RD] = dest;
                                        }
                                        break;

                                case 0x28: /* ADD imm */
                                        dest = GETADDR(RN) + rotate2(opcode);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x29: /* ADDS imm */
                                        lhs = GETADDR(RN);
                                        rhs = rotate2(opcode);
                                        dest = lhs + rhs;
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setadd(lhs, rhs, dest);
                                                arm.reg[RD] = dest;
                                        }
                                        break;

                                case 0x2A: /* ADC imm */
                                        dest = GETADDR(RN) + rotate2(opcode) + CFSET;
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x2B: /* ADCS imm */
                                        lhs = GETADDR(RN);
                                        rhs = rotate2(opcode);
                                        dest = lhs + rhs + CFSET;
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setadc(lhs, rhs, dest);
                                                arm.reg[RD] = dest;
                                        }
                                        break;

                                case 0x2C: /* SBC imm */
                                        dest = GETADDR(RN) - rotate2(opcode) - ((CFSET) ? 0 : 1);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x2D: /* SBCS imm */
                                        lhs = GETADDR(RN);
                                        rhs = rotate2(opcode);
                                        dest = lhs - rhs - (CFSET ? 0 : 1);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setsbc(lhs, rhs, dest);
                                                arm.reg[RD] = dest;
                                        }
                                        break;

                                case 0x2E: /* RSC imm */
                                        dest = rotate2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
                                        arm_write_dest(opcode, dest);
                                        break;

                                case 0x2F: /* RSCS imm */
                                        lhs = GETADDR(RN);
                                        rhs = rotate2(opcode);
                                        dest = rhs - lhs - (CFSET ? 0 : 1);
                                        if (RD==15)
                                        {
                                                arm_write_r15(opcode, dest);
                                        }
                                        else
                                        {
                                                setsbc(rhs, lhs, dest);
                                                arm.reg[RD] = dest;
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
                                                dest = lhs | rotate(opcode);
                                                arm.reg[RD] = dest;
                                                setzn(dest);
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
                                                arm.reg[RD] = rotate(opcode);
                                                setzn(arm.reg[RD]);
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
                                                dest = lhs & ~rotate(opcode);
                                                arm.reg[RD] = dest;
                                                setzn(dest);
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
                                                arm.reg[RD] = ~rotate(opcode);
                                                setzn(arm.reg[RD]);
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
					writememl(addr & ~3, arm.reg[RD]);
					memmode = templ;

					/* Check for Abort */
					if (arm.abort_base_restored && (armirq & 0x40)) {
						break;
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
					arm.reg[RN] = addr;
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
					if (arm.abort_base_restored && (armirq & 0x40)) {
						break;
					}

					/* Rotate if load is unaligned */
					templ2 = arm_ldr_rotate(templ2, addr);

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
					arm.reg[RN] = addr;

					/* Check for Abort (before writing Rd) */
					if (armirq & 0x40) {
						break;
					}

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
					writememb(addr, arm.reg[RD]);
					memmode = templ;

					/* Check for Abort */
					if (arm.abort_base_restored && (armirq & 0x40)) {
						break;
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
					arm.reg[RN] = addr;
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
					if (arm.abort_base_restored && (armirq & 0x40)) {
						break;
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
					arm.reg[RN] = addr;

					/* Check for Abort (before writing Rd) */
					if (armirq & 0x40) {
						break;
					}

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
					templ = GETREG(RD);
					writememl(addr & ~3, templ);

					/* Check for Abort */
					if (arm.abort_base_restored && (armirq & 0x40)) {
						break;
					}

					if (!(opcode & 0x1000000)) {
						/* Post-indexed */
						addr += addr2;
						arm.reg[RN] = addr;
					} else if (opcode & 0x200000) {
						/* Pre-indexed with writeback */
						arm.reg[RN] = addr;
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
					if (arm.abort_base_restored && (armirq & 0x40)) {
						break;
					}

					/* Rotate if load is unaligned */
					templ = arm_ldr_rotate(templ, addr);

					if (!(opcode & 0x1000000)) {
						/* Post-indexed */
						addr += addr2;
						arm.reg[RN] = addr;
					} else if (opcode & 0x200000) {
						/* Pre-indexed with writeback */
						arm.reg[RN] = addr;
					}

					/* Check for Abort (before writing Rd) */
					if (armirq & 0x40) {
						break;
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
					templ = GETREG(RD);
					writememb(addr, templ);

					/* Check for Abort */
					if (arm.abort_base_restored && (armirq & 0x40)) {
						break;
					}

					if (!(opcode & 0x1000000)) {
						/* Post-indexed */
						addr += addr2;
						arm.reg[RN] = addr;
					} else if (opcode & 0x200000) {
						/* Pre-indexed with writeback */
						arm.reg[RN] = addr;
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
					if (arm.abort_base_restored && (armirq & 0x40)) {
						break;
					}

					if (!(opcode & 0x1000000)) {
						/* Post-indexed */
						addr += addr2;
						arm.reg[RN] = addr;
					} else if (opcode & 0x200000) {
						/* Pre-indexed with writeback */
						arm.reg[RN] = addr;
					}

					/* Check for Abort (before writing Rd) */
					if (armirq & 0x40) {
						break;
					}

					/* Write Rd */
					LOADREG(RD, templ);
					break;

				case 0x80: /* STMDA */
				case 0x82: /* STMDA ! */
				case 0x90: /* STMDB */
				case 0x92: /* STMDB ! */
					offset = countbits(opcode & 0xffff);
					addr = arm.reg[RN] - offset;
					writeback = addr;
					if (!(opcode & (1 << 24))) {
						/* Decrement After */
						addr += 4;
					}
					arm_store_multiple(opcode, addr, writeback);
					break;

				case 0x88: /* STMIA */
				case 0x8a: /* STMIA ! */
				case 0x98: /* STMIB */
				case 0x9a: /* STMIB ! */
					offset = countbits(opcode & 0xffff);
					addr = arm.reg[RN];
					writeback = addr + offset;
					if (opcode & (1 << 24)) {
						/* Increment Before */
						addr += 4;
					}
					arm_store_multiple(opcode, addr, writeback);
					break;

				case 0x84: /* STMDA ^ */
				case 0x86: /* STMDA ^! */
				case 0x94: /* STMDB ^ */
				case 0x96: /* STMDB ^! */
					offset = countbits(opcode & 0xffff);
					addr = arm.reg[RN] - offset;
					writeback = addr;
					if (!(opcode & (1 << 24))) {
						/* Decrement After */
						addr += 4;
					}
					arm_store_multiple_s(opcode, addr, writeback);
					break;

				case 0x8c: /* STMIA ^ */
				case 0x8e: /* STMIA ^! */
				case 0x9c: /* STMIB ^ */
				case 0x9e: /* STMIB ^! */
					offset = countbits(opcode & 0xffff);
					addr = arm.reg[RN];
					writeback = addr + offset;
					if (opcode & (1 << 24)) {
						/* Increment Before */
						addr += 4;
					}
					arm_store_multiple_s(opcode, addr, writeback);
					break;

				case 0x81: /* LDMDA */
				case 0x83: /* LDMDA ! */
				case 0x91: /* LDMDB */
				case 0x93: /* LDMDB ! */
					offset = countbits(opcode & 0xffff);
					addr = arm.reg[RN] - offset;
					writeback = addr;
					if (!(opcode & (1 << 24))) {
						/* Decrement After */
						addr += 4;
					}
					arm_load_multiple(opcode, addr, writeback);
					break;

				case 0x89: /* LDMIA */
				case 0x8b: /* LDMIA ! */
				case 0x99: /* LDMIB */
				case 0x9b: /* LDMIB ! */
					offset = countbits(opcode & 0xffff);
					addr = arm.reg[RN];
					writeback = addr + offset;
					if (opcode & (1 << 24)) {
						/* Increment Before */
						addr += 4;
					}
					arm_load_multiple(opcode, addr, writeback);
					break;

				case 0x85: /* LDMDA ^ */
				case 0x87: /* LDMDA ^! */
				case 0x95: /* LDMDB ^ */
				case 0x97: /* LDMDB ^! */
					offset = countbits(opcode & 0xffff);
					addr = arm.reg[RN] - offset;
					writeback = addr;
					if (!(opcode & (1 << 24))) {
						/* Decrement After */
						addr += 4;
					}
					arm_load_multiple_s(opcode, addr, writeback);
					break;

				case 0x8d: /* LDMIA ^ */
				case 0x8f: /* LDMIA ^! */
				case 0x9d: /* LDMIB ^ */
				case 0x9f: /* LDMIB ^! */
					offset = countbits(opcode & 0xffff);
					addr = arm.reg[RN];
					writeback = addr + offset;
					if (opcode & (1 << 24)) {
						/* Increment Before */
						addr += 4;
					}
					arm_load_multiple_s(opcode, addr, writeback);
					break;

                                case 0xA0: case 0xA1: case 0xA2: case 0xA3: /* B */
                                case 0xA4: case 0xA5: case 0xA6: case 0xA7:
                                case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                                case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                                        /* Extract offset bits, and sign-extend */
                                        templ = (opcode << 8);
                                        templ = (uint32_t) ((int32_t) templ >> 6);
                                        arm.reg[15] = ((arm.reg[15] + templ + 4) & r15mask) |
                                                      (arm.reg[15]&~r15mask);
                                        break;

                                case 0xB0: case 0xB1: case 0xB2: case 0xB3: /* BL */
                                case 0xB4: case 0xB5: case 0xB6: case 0xB7:
                                case 0xB8: case 0xB9: case 0xBA: case 0xBB:
                                case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                                        /* Extract offset bits, and sign-extend */
                                        templ = (opcode << 8);
                                        templ = (uint32_t) ((int32_t) templ >> 6);
                                        arm.reg[14] = arm.reg[15] - 4;
                                        arm.reg[15] = ((arm.reg[15] + templ + 4) & r15mask) |
                                                      (arm.reg[15] & ~r15mask);
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
                                                writecp15(RN, arm.reg[RD], opcode);
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
                                                if (RD==15) arm.reg[RD] = (arm.reg[RD] & r15mask) | (readcp15(RN) & ~r15mask);
                                                else        arm.reg[RD] = readcp15(RN);
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

				case 0xF0: case 0xF1: case 0xF2: case 0xF3: /* SWI */
				case 0xF4: case 0xF5: case 0xF6: case 0xF7:
				case 0xF8: case 0xF9: case 0xFA: case 0xFB:
				case 0xFC: case 0xFD: case 0xFE: case 0xFF:
					opSWI(opcode);
					break;

				default:
					bad_opcode(opcode);
					break;
                                }
                        }
#ifdef STRONGARM
                        }
#endif
                        if (/*databort|*/armirq)//|prefabort)
                        {
                                if (!ARM_MODE_32(arm.mode)) {
                                        arm.reg[16] &= ~0xc0;
                                        arm.reg[16] |= ((arm.reg[15] & 0xc000000) >> 20);
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
                                        fatal("Exception %i %i %i\n", databort, armirq, prefabort);

                                        templ = arm.reg[15];
                                        arm.reg[15] |= 3;
                                        updatemode(SUPERVISOR);
                                        arm.reg[14] = templ;
                                        arm.reg[15] &= 0xfc000003;
                                        arm.reg[15] |= 0x08000018;
                                        refillpipeline();
                                        databort=0;
                                }
//                                #endif
                                }
                                else if ((armirq & 2) && !(arm.reg[16] & 0x40)) /*FIQ*/
                                {
                                        exception(FIQ,0x20,0);
                                }
                                else if ((armirq & 1) && !(arm.reg[16] & 0x80)) /*IRQ*/
                                {
                                        exception(IRQ, 0x1c, 0);
                                }
//                                if ((arm.reg[cpsr]&arm.mmask)!=arm.mode) updatemode(arm.reg[cpsr]&arm.mmask);
                        }

                        arm.reg[15] += 4;

//                        if ((arm.reg[cpsr]&arm.mmask)!=arm.mode) updatemode(arm.reg[cpsr]&arm.mmask);

//                        linecyc--;
//                        inscount++;
                }
                inscount+=200;

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
                        fdccallback-=10;
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
//                cyc=(oldcyc-cycles);
                cycles-=200;
        }
}
#endif
