/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2005-2010 Sarah Walker

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*Preliminary FPA emulation. This works to an extent - !Draw works with it, !SICK
  seems to (FPA Whetstone scores are around 100x without), but !AMPlayer doesn't
  work, and GCC stuff tends to crash.*/
//#define FPA

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "rpcemu.h"
#include "arm.h"
#include "cp15.h"
#include "mem.h"

ARMState arm;

int blockend;
uint32_t inscount;
int cpsr;
static uint32_t *pcpsr;

static uint8_t flaglookup[16][16];

uint32_t *usrregs[16];
int prog32;

static int unpredictable_count = 1000; ///< Limit logging of unpredictable instructions

#define NFSET	((arm.reg[cpsr] & NFLAG) ? 1u : 0)
#define ZFSET	((arm.reg[cpsr] & ZFLAG) ? 1u : 0)
#define CFSET	((arm.reg[cpsr] & CFLAG) ? 1u : 0)
#define VFSET	((arm.reg[cpsr] & VFLAG) ? 1u : 0)

#include "arm_common.h"

#undef refillpipeline
#define refillpipeline()

uint32_t pccache;
static const uint32_t *pccache2;

/**
 * Return true if this ARM core is the dynarec version
 *
 * @return 0 no this isn't dynarec
 */
int
arm_is_dynarec(void)
{
	return 0;
}

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
                arm.r15_mask = 0xfffffffc;
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
                arm.r15_mask = 0x3fffffc;
                arm.reg[16] = (arm.reg[16] & 0xffffffe0) | arm.mode;
                if (ARM_MODE_32(om)) {
                        arm.reg[15] &= arm.r15_mask;
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
	unsigned c, d, exec;

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
}

/**
 * Reset the ARM core to initial state. The CPU model must be selected at this
 * point.
 *
 * @param cpu_model CPU model to emulate
 */
void
arm_reset(CPUModel cpu_model)
{
	memset(&arm, 0, sizeof(arm));

        arm.r15_mask = 0x3fffffc;
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
		arm.arch_v4 = 1;
	} else {
		arm.r15_diff = 4;
		arm.abort_base_restored = 0;
		arm.stm_writeback_at_end = 0;
		arm.arch_v4 = 0;
	}
}

void
arm_dump(void)
{
	char s[1024];

	sprintf(s, "r0 = %08x    r4 = %08x    r8  = %08x    r12 = %08x\n"
	           "r1 = %08x    r5 = %08x    r9  = %08x    r13 = %08x\n"
	           "r2 = %08x    r6 = %08x    r10 = %08x    r14 = %08x\n"
	           "r3 = %08x    r7 = %08x    r11 = %08x    r15 = %08x\n"
	           "pc = %08x\n"
	           "%s\n",
	           arm.reg[0], arm.reg[4], arm.reg[8], arm.reg[12],
	           arm.reg[1], arm.reg[5], arm.reg[9], arm.reg[13],
	           arm.reg[2], arm.reg[6], arm.reg[10], arm.reg[14],
	           arm.reg[3], arm.reg[7], arm.reg[11], arm.reg[15],
	           PC,
	           mmu ? "MMU enabled" : "MMU disabled");
	rpclog("%s", s);
	printf("%s", s);
}

static uint32_t
shift3(uint32_t opcode)
{
	uint32_t shiftmode = opcode & 0x60;
	uint32_t shiftamount;
	uint32_t temp;
	uint32_t cflag = CFSET;

	if (opcode & 0x10) {
		shiftamount = arm.reg[(opcode >> 8) & 0xf] & 0xff;
	} else {
		shiftamount = (opcode >> 7) & 0x1f;
	}
	temp = arm.reg[RM];
	if (shiftamount != 0) {
		arm.reg[cpsr] &= ~CFLAG;
	}
	switch (shiftmode) {
	case 0: /* LSL */
		if (shiftamount == 0) {
			return temp;
		}
		if (shiftamount == 32) {
			if (temp & 1) {
				arm.reg[cpsr] |= CFLAG;
			}
			return 0;
		}
		if (shiftamount > 32) {
			return 0;
		}
		if ((temp << (shiftamount - 1)) & 0x80000000) {
			arm.reg[cpsr] |= CFLAG;
		}
		return temp << shiftamount;

	case 0x20: /* LSR */
		if (shiftamount == 0 && !(opcode & 0x10)) {
			shiftamount = 32;
		}
		if (shiftamount == 0) {
			return temp;
		}
		if (shiftamount == 32) {
			if (temp & 0x80000000) {
				arm.reg[cpsr] |= CFLAG;
			} else {
				arm.reg[cpsr] &= ~CFLAG;
			}
			return 0;
		}
		if (shiftamount > 32) {
			return 0;
		}
		if ((temp >> (shiftamount - 1)) & 1) {
			arm.reg[cpsr] |= CFLAG;
		}
		return temp >> shiftamount;

	case 0x40: /* ASR */
		if (shiftamount == 0) {
			if (opcode & 0x10) {
				return temp;
			}
		}
		if (shiftamount >= 32 || shiftamount == 0) {
			if (temp & 0x80000000) {
				arm.reg[cpsr] |= CFLAG;
			} else {
				arm.reg[cpsr] &= ~CFLAG;
			}
			if (temp & 0x80000000) {
				return 0xffffffff;
			}
			return 0;
		}
		if (((int32_t) temp >> (shiftamount - 1)) & 1) {
			arm.reg[cpsr] |= CFLAG;
		}
		return (uint32_t) ((int32_t) temp >> shiftamount);

	default: /* ROR */
		arm.reg[cpsr] &= ~CFLAG;
		if (shiftamount == 0 && !(opcode & 0x10)) {
			/* RRX */
			if (temp & 1) {
				arm.reg[cpsr] |= CFLAG;
			}
			return (cflag << 31) | (temp >> 1);
		}
		if (shiftamount == 0) {
			arm.reg[cpsr] |= (cflag << 29);
			return temp;
		}
		if ((shiftamount & 0x1f) == 0) {
			if (temp & 0x80000000) {
				arm.reg[cpsr] |= CFLAG;
			}
			return temp;
		}
		shiftamount &= 0x1f;
		if (rotate_right32(temp, shiftamount) & 0x80000000) {
			arm.reg[cpsr] |= CFLAG;
		}
		return rotate_right32(temp, shiftamount);
	}
}

#define shift(o)  ((o & 0xff0) ? shift3(o) : arm.reg[RM])
#define shift2(o) ((o & 0xff0) ? shift4(o) : arm.reg[RM])
#define shift_ldrstr(o) shift2(o)

static uint32_t
shift5(uint32_t opcode, uint32_t shiftmode, uint32_t shiftamount, uint32_t rm)
{
	switch (shiftmode) {
	case 0: /* LSL */
		if (shiftamount == 0) {
			return rm;
		}
		return 0; /* shiftamount >= 32 */

	case 0x20: /* LSR */
		if (shiftamount == 0 && (opcode & 0x10)) {
			return rm;
		}
		return 0; /* shiftamount >= 32 */

	case 0x40: /* ASR */
		if (shiftamount == 0 && !(opcode & 0x10)) {
			shiftamount = 32;
		}
		if (shiftamount >= 32) {
			if (rm & 0x80000000) {
				return 0xffffffff;
			}
			return 0;
		}
		return (uint32_t) ((int32_t) rm >> shiftamount);

	default: /* ROR */
		if (!(opcode & 0x10)) {
			/* RRX */
			return (CFSET << 31) | (rm >> 1);
		}
		shiftamount &= 0x1f;
		return rotate_right32(rm, shiftamount);
	}
}

static inline uint32_t
shift4(uint32_t opcode)
{
	uint32_t shiftmode = opcode & 0x60;
	uint32_t shiftamount;
	uint32_t rm = arm.reg[RM];

	if (opcode & 0x10) {
		shiftamount = arm.reg[(opcode >> 8) & 0xf] & 0xff;
	} else {
		shiftamount = (opcode >> 7) & 0x1f;
	}

	if ((shiftamount - 1) >= 31) {
		return shift5(opcode, shiftmode, shiftamount, rm);
	}

	switch (shiftmode) {
	case 0: /* LSL */
		return rm << shiftamount;
	case 0x20: /* LSR */
		return rm >> shiftamount;
	case 0x40: /* ASR */
		return (uint32_t) ((int32_t) rm >> shiftamount);
	default: /* ROR */
		return rotate_right32(rm, shiftamount);
	}
}

void
exception(uint32_t mmode, uint32_t address, uint32_t diff)
{
	uint32_t link;
	uint32_t irq_disable;

	/* If FIQ exception, disable FIQ and IRQ, otherwise disable just IRQ */
	if (mmode == FIQ) {
		irq_disable = (0x80 | 0x40);
	} else {
		irq_disable = 0x80;
	}

	link = arm.reg[15] - diff;

	if (ARM_MODE_32(arm.mode)) {
		arm.spsr[mmode] = arm.reg[16];
		updatemode(0x10 | mmode);
		arm.reg[14] = link;
		arm.reg[16] &= ~0x1fu;
		arm.reg[16] |= 0x10 | mmode | irq_disable;
		arm.reg[15] = address;
	} else if (prog32) {
		updatemode(0x10 | mmode);
		arm.reg[14] = link & 0x3fffffc;
		arm.spsr[mmode] = (arm.reg[16] & ~0x1fu) | (link & 3);
		arm.spsr[mmode] &= ~0x10u;
		arm.reg[16] |= irq_disable;
		arm.reg[15] = address;
	} else {
		arm.reg[15] |= 3;
		/* When in 26-bit config, Abort and Undefined exceptions enter
		   mode SVC_26 */
		updatemode(mmode >= SUPERVISOR ? SUPERVISOR : mmode);
		arm.reg[14] = link;
		arm.reg[15] &= 0xfc000003;
		arm.reg[15] |= ((irq_disable << 20) | address);
	}
}

/**
 * An instruction with unpredictable behaviour has been encountered.
 *
 * On real hardware these can have very odd behaviour, so log these in case
 * software is depending on them.
 *
 * @param opcode Opcode of instruction being emulated
 */
static void
arm_unpredictable(uint32_t opcode)
{
	if (unpredictable_count != 0) {
		unpredictable_count--;
		rpclog("ARM: Unpredictable opcode %08x at %08x\n", opcode, PC);
	}
}

/**
 * Execute several ARM instructions.
 *
 * @return A hint roughly proportional to the amount of instructions executed.
 */
int
arm_exec(void)
{
	int linecyc;

	for (linecyc = 0; linecyc < 200; linecyc++) {
		uint32_t opcode;
		uint32_t lhs, rhs, dest;
		uint32_t addr, data, offset, writeback;

		if ((PC >> 12) != pccache) {
			pccache = PC >> 12;
			pccache2 = getpccache(PC);
			if (pccache2 == NULL) {
				// Prefetch Abort
				pccache = 0xffffffff;
				exception(ABORT, 0x10, 4);
				arm.reg[15] += 4;
				continue;
			}
		}
		opcode = pccache2[PC >> 2];

		if (flaglookup[opcode >> 28][(*pcpsr) >> 28]) {
			if (arm.arch_v4) {
				if ((opcode & 0xe0000f0) == 0xb0) {
					// LDRH/STRH
					if (opcode & 0x100000) {
						arm_ldrh(opcode);
					} else {
						arm_strh(opcode);
					}
					goto skip;
				} else if ((opcode & 0xe1000d0) == 0x1000d0) {
					// LDRSB/LDRSH
					if ((opcode & 0xf0) == 0xd0) {
						arm_ldrsb(opcode);
					} else {
						arm_ldrsh(opcode);
					}
					goto skip;
				}
			}

			switch ((opcode >> 20) & 0xff) {
			case 0x00: // AND reg
				if ((opcode & 0xf0) == 0x90) {
					// MUL
					arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
					    (arm.reg[MULRM] * arm.reg[MULRS]);
				} else {
					dest = GETADDR(RN) & shift2(opcode);
					arm_write_dest(opcode, dest);
				}
				break;

			case 0x01: // ANDS reg
				if ((opcode & 0xf0) == 0x90) {
					// MULS
					arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
					    (arm.reg[MULRM] * arm.reg[MULRS]);
					arm_flags_logical(arm.reg[MULRD]);
				} else {
					lhs = GETADDR(RN);
					if (RD == 15) {
						arm_write_r15(opcode, lhs & shift2(opcode));
					} else {
						dest = lhs & shift(opcode);
						arm.reg[RD] = dest;
						arm_flags_logical(dest);
					}
				}
				break;

			case 0x02: // EOR reg
				if ((opcode & 0xf0) == 0x90) {
					// MLA
					arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
					    (arm.reg[MULRM] * arm.reg[MULRS]) + arm.reg[MULRN];
				} else {
					dest = GETADDR(RN) ^ shift2(opcode);
					arm_write_dest(opcode, dest);
				}
				break;

			case 0x03: // EORS reg
				if ((opcode & 0xf0) == 0x90) {
					// MLAS
					arm.reg[MULRD] = (MULRD == MULRM) ? 0 :
					    (arm.reg[MULRM] * arm.reg[MULRS]) + arm.reg[MULRN];
					arm_flags_logical(arm.reg[MULRD]);
				} else {
					lhs = GETADDR(RN);
					if (RD == 15) {
						arm_write_r15(opcode, lhs ^ shift2(opcode));
					} else {
						dest = lhs ^ shift(opcode);
						arm.reg[RD] = dest;
						arm_flags_logical(dest);
					}
				}
				break;

			case 0x04: // SUB reg
				dest = GETADDR(RN) - shift2(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x05: // SUBS reg
				lhs = GETADDR(RN);
				rhs = shift2(opcode);
				dest = lhs - rhs;
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_sub(lhs, rhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x06: // RSB reg
				dest = shift2(opcode) - GETADDR(RN);
				arm_write_dest(opcode, dest);
				break;

			case 0x07: // RSBS reg
				lhs = GETADDR(RN);
				rhs = shift2(opcode);
				dest = rhs - lhs;
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_sub(rhs, lhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x08: // ADD reg
				if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
					// UMULL
					uint64_t mula = (uint64_t) arm.reg[MULRS];
					uint64_t mulb = (uint64_t) arm.reg[MULRM];
					uint64_t mulres = mula * mulb;

					arm.reg[MULRN] = (uint32_t) mulres;
					arm.reg[MULRD] = (uint32_t) (mulres >> 32);
					break;
				}
				dest = GETADDR(RN) + shift2(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x09: // ADDS reg
				if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
					// UMULLS
					uint64_t mula = (uint64_t) arm.reg[MULRS];
					uint64_t mulb = (uint64_t) arm.reg[MULRM];
					uint64_t mulres = mula * mulb;

					arm.reg[MULRN] = (uint32_t) mulres;
					arm.reg[MULRD] = (uint32_t) (mulres >> 32);
					arm_flags_long_multiply(mulres);
					break;
				}
				lhs = GETADDR(RN);
				rhs = shift2(opcode);
				dest = lhs + rhs;
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_add(lhs, rhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x0a: // ADC reg
				if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
					// UMLAL
					uint64_t mula = (uint64_t) arm.reg[MULRS];
					uint64_t mulb = (uint64_t) arm.reg[MULRM];
					uint64_t current = ((uint64_t) arm.reg[MULRD] << 32) |
					                   arm.reg[MULRN];
					uint64_t mulres = (mula * mulb) + current;

					arm.reg[MULRN] = (uint32_t) mulres;
					arm.reg[MULRD] = (uint32_t) (mulres >> 32);
					break;
				}
				dest = GETADDR(RN) + shift2(opcode) + CFSET;
				arm_write_dest(opcode, dest);
				break;

			case 0x0b: // ADCS reg
				if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
					// UMLALS
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
				lhs = GETADDR(RN);
				rhs = shift2(opcode);
				dest = lhs + rhs + CFSET;
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_adc(lhs, rhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x0c: // SBC reg
				if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
					// SMULL
					int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
					int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
					int64_t mulres = mula * mulb;

					arm.reg[MULRN] = (uint32_t) mulres;
					arm.reg[MULRD] = (uint32_t) (mulres >> 32);
					break;
				}
				dest = GETADDR(RN) - shift2(opcode) - ((CFSET) ? 0 : 1);
				arm_write_dest(opcode, dest);
				break;

			case 0x0d: // SBCS reg
				if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
					// SMULLS
					int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
					int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
					int64_t mulres = mula * mulb;

					arm.reg[MULRN] = (uint32_t) mulres;
					arm.reg[MULRD] = (uint32_t) (mulres >> 32);
					arm_flags_long_multiply(mulres);
					break;
				}
				lhs = GETADDR(RN);
				rhs = shift2(opcode);
				dest = lhs - rhs - (CFSET ? 0 : 1);
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_sbc(lhs, rhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x0e: // RSC reg
				if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
					// SMLAL
					int64_t mula = (int64_t) (int32_t) arm.reg[MULRS];
					int64_t mulb = (int64_t) (int32_t) arm.reg[MULRM];
					int64_t current = ((int64_t) arm.reg[MULRD] << 32) |
					                  arm.reg[MULRN];
					int64_t mulres = (mula * mulb) + current;

					arm.reg[MULRN] = (uint32_t) mulres;
					arm.reg[MULRD] = (uint32_t) (mulres >> 32);
					break;
				}
				dest = shift2(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
				arm_write_dest(opcode, dest);
				break;

			case 0x0f: // RSCS reg
				if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
					// SMLALS
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
				lhs = GETADDR(RN);
				rhs = shift2(opcode);
				dest = rhs - lhs - (CFSET ? 0 : 1);
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_sbc(rhs, lhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x10: // MRS reg,CPSR and SWP
				if ((opcode & 0xff0) == 0x90) {
					// SWP
					if (RD != 15) {
						addr = GETADDR(RN);
						data = GETREG(RM);
						dest = mem_read32(addr & ~3u);
						if (arm.event & 0x40) {
							break;
						}
						dest = arm_ldr_rotate(dest, addr);
						mem_write32(addr & ~3u, data);
						if (arm.event & 0x40) {
							break;
						}
						LOADREG(RD, dest);
					}
				} else if ((opcode & 0xf0fff) == 0xf0000) {
					// MRS reg,CPSR
					if (!ARM_MODE_32(arm.mode)) {
						arm.reg[16] = (arm.reg[15] & 0xf0000000) | (arm.reg[15] & 3);
						arm.reg[16] |= ((arm.reg[15] & 0xc000000) >> 20);
					}
					arm.reg[RD] = arm.reg[16];
				} else if (arm.arch_v4) {
					arm_exception_undefined();
				} else {
					arm_unpredictable(opcode);
				}
				break;

			case 0x11: // TST reg
				lhs = GETADDR(RN);
				if (RD == 15) {
					// TSTP reg
					arm_compare_rd15(opcode, lhs & shift2(opcode));
				} else {
					arm_flags_logical(lhs & shift(opcode));
				}
				break;

			case 0x12: // MSR CPSR,reg
				if ((opcode & 0xf010) == 0xf000) {
					arm_write_cpsr(opcode, arm.reg[RM]);
				} else if (arm.arch_v4) {
					arm_exception_undefined();
				} else {
					arm_unpredictable(opcode);
				}
				break;

			case 0x13: // TEQ reg
				lhs = GETADDR(RN);
				if (RD == 15) {
					// TEQP reg
					arm_compare_rd15(opcode, lhs ^ shift2(opcode));
				} else {
					arm_flags_logical(lhs ^ shift(opcode));
				}
				break;

			case 0x14: // MRS reg,SPSR and SWPB
				if ((opcode & 0xff0) == 0x90) {
					// SWPB
					if (RD != 15) {
						addr = GETADDR(RN);
						data = GETREG(RM);
						dest = mem_read8(addr);
						if (arm.event & 0x40) {
							break;
						}
						mem_write8(addr, data);
						if (arm.event & 0x40) {
							break;
						}
						LOADREG(RD, dest);
					}
				} else if ((opcode & 0xf0fff) == 0xf0000) {
					// MRS reg,SPSR
					arm.reg[RD] = arm_read_spsr();
				} else if (arm.arch_v4) {
					arm_exception_undefined();
				} else {
					arm_unpredictable(opcode);
				}
				break;

			case 0x15: // CMP reg
				lhs = GETADDR(RN);
				rhs = shift2(opcode);
				dest = lhs - rhs;
				if (RD == 15) {
					// CMPP reg
					arm_compare_rd15(opcode, dest);
				} else {
					arm_flags_sub(lhs, rhs, dest);
				}
				break;

			case 0x16: // MSR SPSR,reg
				if ((opcode & 0xf010) == 0xf000) {
					arm_write_spsr(opcode, arm.reg[RM]);
				} else if (arm.arch_v4) {
					arm_exception_undefined();
				} else {
					arm_unpredictable(opcode);
				}
				break;

			case 0x17: // CMN reg
				lhs = GETADDR(RN);
				rhs = shift2(opcode);
				dest = lhs + rhs;
				if (RD == 15) {
					// CMNP reg
					arm_compare_rd15(opcode, dest);
				} else {
					arm_flags_add(lhs, rhs, dest);
				}
				break;

			case 0x18: // ORR reg
				dest = GETADDR(RN) | shift2(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x19: // ORRS reg
				lhs = GETADDR(RN);
				if (RD == 15) {
					arm_write_r15(opcode, lhs | shift2(opcode));
				} else {
					dest = lhs | shift(opcode);
					arm.reg[RD] = dest;
					arm_flags_logical(dest);
				}
				break;

			case 0x1a: // MOV reg
				dest = shift2(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x1b: // MOVS reg
				if (RD == 15) {
					arm_write_r15(opcode, shift2(opcode));
				} else {
					arm.reg[RD] = shift(opcode);
					arm_flags_logical(arm.reg[RD]);
				}
				break;

			case 0x1c: // BIC reg
				dest = GETADDR(RN) & ~shift2(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x1d: // BICS reg
				lhs = GETADDR(RN);
				if (RD == 15) {
					arm_write_r15(opcode, lhs & ~shift2(opcode));
				} else {
					dest = lhs & ~shift(opcode);
					arm.reg[RD] = dest;
					arm_flags_logical(dest);
				}
				break;

			case 0x1e: // MVN reg
				dest = ~shift2(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x1f: // MVNS reg
				if (RD == 15) {
					arm_write_r15(opcode, ~shift2(opcode));
				} else {
					arm.reg[RD] = ~shift(opcode);
					arm_flags_logical(arm.reg[RD]);
				}
				break;

			case 0x20: // AND imm
				dest = GETADDR(RN) & arm_imm(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x21: // ANDS imm
				lhs = GETADDR(RN);
				if (RD == 15) {
					arm_write_r15(opcode, lhs & arm_imm(opcode));
				} else {
					dest = lhs & arm_imm_cflag(opcode);
					arm.reg[RD] = dest;
					arm_flags_logical(dest);
				}
				break;

			case 0x22: // EOR imm
				dest = GETADDR(RN) ^ arm_imm(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x23: // EORS imm
				lhs = GETADDR(RN);
				if (RD == 15) {
					arm_write_r15(opcode, lhs ^ arm_imm(opcode));
				} else {
					dest = lhs ^ arm_imm_cflag(opcode);
					arm.reg[RD] = dest;
					arm_flags_logical(dest);
				}
				break;

			case 0x24: // SUB imm
				dest = GETADDR(RN) - arm_imm(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x25: // SUBS imm
				lhs = GETADDR(RN);
				rhs = arm_imm(opcode);
				dest = lhs - rhs;
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm.reg[RD] = dest;
					arm_flags_sub(lhs, rhs, dest);
				}
				break;

			case 0x26: // RSB imm
				dest = arm_imm(opcode) - GETADDR(RN);
				arm_write_dest(opcode, dest);
				break;

			case 0x27: // RSBS imm
				lhs = GETADDR(RN);
				rhs = arm_imm(opcode);
				dest = rhs - lhs;
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_sub(rhs, lhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x28: // ADD imm
				dest = GETADDR(RN) + arm_imm(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x29: // ADDS imm
				lhs = GETADDR(RN);
				rhs = arm_imm(opcode);
				dest = lhs + rhs;
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_add(lhs, rhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x2a: // ADC imm
				dest = GETADDR(RN) + arm_imm(opcode) + CFSET;
				arm_write_dest(opcode, dest);
				break;

			case 0x2b: // ADCS imm
				lhs = GETADDR(RN);
				rhs = arm_imm(opcode);
				dest = lhs + rhs + CFSET;
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_adc(lhs, rhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x2c: // SBC imm
				dest = GETADDR(RN) - arm_imm(opcode) - ((CFSET) ? 0 : 1);
				arm_write_dest(opcode, dest);
				break;

			case 0x2d: // SBCS imm
				lhs = GETADDR(RN);
				rhs = arm_imm(opcode);
				dest = lhs - rhs - (CFSET ? 0 : 1);
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_sbc(lhs, rhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x2e: // RSC imm
				dest = arm_imm(opcode) - GETADDR(RN) - ((CFSET) ? 0 : 1);
				arm_write_dest(opcode, dest);
				break;

			case 0x2f: // RSCS imm
				lhs = GETADDR(RN);
				rhs = arm_imm(opcode);
				dest = rhs - lhs - (CFSET ? 0 : 1);
				if (RD == 15) {
					arm_write_r15(opcode, dest);
				} else {
					arm_flags_sbc(rhs, lhs, dest);
					arm.reg[RD] = dest;
				}
				break;

			case 0x31: // TST imm
				lhs = GETADDR(RN);
				if (RD == 15) {
					// TSTP imm
					arm_compare_rd15(opcode, lhs & arm_imm(opcode));
				} else {
					arm_flags_logical(lhs & arm_imm_cflag(opcode));
				}
				break;

			case 0x32: // MSR CPSR,imm
				if (RD == 15) {
					arm_write_cpsr(opcode, arm_imm(opcode));
				} else if (arm.arch_v4) {
					arm_exception_undefined();
				} else {
					arm_unpredictable(opcode);
				}
				break;

			case 0x33: // TEQ imm
				lhs = GETADDR(RN);
				if (RD == 15) {
					// TEQP imm
					arm_compare_rd15(opcode, lhs ^ arm_imm(opcode));
				} else {
					arm_flags_logical(lhs ^ arm_imm_cflag(opcode));
				}
				break;

			case 0x35: // CMP imm
				lhs = GETADDR(RN);
				rhs = arm_imm(opcode);
				dest = lhs - rhs;
				if (RD == 15) {
					// CMPP imm
					arm_compare_rd15(opcode, dest);
				} else {
					arm_flags_sub(lhs, rhs, dest);
				}
				break;

			case 0x36: // MSR SPSR,imm
				if (RD == 15) {
					arm_write_spsr(opcode, arm_imm(opcode));
				} else if (arm.arch_v4) {
					arm_exception_undefined();
				} else {
					arm_unpredictable(opcode);
				}
				break;

			case 0x37: // CMN imm
				lhs = GETADDR(RN);
				rhs = arm_imm(opcode);
				dest = lhs + rhs;
				if (RD == 15) {
					// CMNP imm
					arm_compare_rd15(opcode, dest);
				} else {
					arm_flags_add(lhs, rhs, dest);
				}
				break;

			case 0x38: // ORR imm
				dest = GETADDR(RN) | arm_imm(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x39: // ORRS imm
				lhs = GETADDR(RN);
				if (RD == 15) {
					arm_write_r15(opcode, lhs | arm_imm(opcode));
				} else {
					dest = lhs | arm_imm_cflag(opcode);
					arm.reg[RD] = dest;
					arm_flags_logical(dest);
				}
				break;

			case 0x3a: // MOV imm
				dest = arm_imm(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x3b: // MOVS imm
				if (RD == 15) {
					arm_write_r15(opcode, arm_imm(opcode));
				} else {
					arm.reg[RD] = arm_imm_cflag(opcode);
					arm_flags_logical(arm.reg[RD]);
				}
				break;

			case 0x3c: // BIC imm
				dest = GETADDR(RN) & ~arm_imm(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x3d: // BICS imm
				lhs = GETADDR(RN);
				if (RD == 15) {
					arm_write_r15(opcode, lhs & ~arm_imm(opcode));
				} else {
					dest = lhs & ~arm_imm_cflag(opcode);
					arm.reg[RD] = dest;
					arm_flags_logical(dest);
				}
				break;

			case 0x3e: // MVN imm
				dest = ~arm_imm(opcode);
				arm_write_dest(opcode, dest);
				break;

			case 0x3f: // MVNS imm
				if (RD == 15) {
					arm_write_r15(opcode, ~arm_imm(opcode));
				} else {
					arm.reg[RD] = ~arm_imm_cflag(opcode);
					arm_flags_logical(arm.reg[RD]);
				}
				break;

			case 0x62: // STRT Rd, [Rn], -reg...
			case 0x6a: // STRT Rd, [Rn], +reg...
				if (opcode & 0x10) {
					arm_exception_undefined();
					break;
				}
				// Fall-through
			case 0x42: // STRT Rd, [Rn], #-imm
			case 0x4a: // STRT Rd, [Rn], #+imm
				addr = GETADDR(RN);

				// Store with User mode privileges
				data = GETREG(RD);
				mem_user_write32(addr & ~3u, data);

				// Check for Abort
				if (arm.abort_base_restored && (arm.event & 0x40)) {
					break;
				}

				// Writeback
				if (opcode & 0x2000000) {
					offset = shift_ldrstr(opcode);
				} else {
					offset = opcode & 0xfff;
				}
				if (!(opcode & 0x800000)) {
					offset = -offset;
				}
				addr += offset;
				arm.reg[RN] = addr;
				break;

			case 0x63: // LDRT Rd, [Rn], -reg...
			case 0x6b: // LDRT Rd, [Rn], +reg...
				if (opcode & 0x10) {
					arm_exception_undefined();
					break;
				}
				// Fall-through
			case 0x43: // LDRT Rd, [Rn], #-imm
			case 0x4b: // LDRT Rd, [Rn], #+imm
				addr = GETADDR(RN);

				// Load with User mode privileges
				data = mem_user_read32(addr & ~3u);

				// Check for Abort
				if (arm.abort_base_restored && (arm.event & 0x40)) {
					break;
				}

				// Rotate if load is unaligned
				data = arm_ldr_rotate(data, addr);

				// Writeback
				if (opcode & 0x2000000) {
					offset = shift_ldrstr(opcode);
				} else {
					offset = opcode & 0xfff;
				}
				if (!(opcode & 0x800000)) {
					offset = -offset;
				}
				addr += offset;
				arm.reg[RN] = addr;

				// Check for Abort (before writing Rd)
				if (arm.event & 0x40) {
					break;
				}

				// Write Rd
				LOADREG(RD, data);
				break;

			case 0x66: // STRBT Rd, [Rn], -reg...
			case 0x6e: // STRBT Rd, [Rn], +reg...
				if (opcode & 0x10) {
					arm_exception_undefined();
					break;
				}
				// Fall-through
			case 0x46: // STRBT Rd, [Rn], #-imm
			case 0x4e: // STRBT Rd, [Rn], #+imm
				addr = GETADDR(RN);

				// Store with User mode privileges
				data = GETREG(RD);
				mem_user_write8(addr, data);

				// Check for Abort
				if (arm.abort_base_restored && (arm.event & 0x40)) {
					break;
				}

				// Writeback
				if (opcode & 0x2000000) {
					offset = shift_ldrstr(opcode);
				} else {
					offset = opcode & 0xfff;
				}
				if (!(opcode & 0x800000)) {
					offset = -offset;
				}
				addr += offset;
				arm.reg[RN] = addr;
				break;

			case 0x67: // LDRBT Rd, [Rn], -reg...
			case 0x6f: // LDRBT Rd, [Rn], +reg...
				if (opcode & 0x10) {
					arm_exception_undefined();
					break;
				}
				// Fall-through
			case 0x47: // LDRBT Rd, [Rn], #-imm
			case 0x4f: // LDRBT Rd, [Rn], #+imm
				addr = GETADDR(RN);

				// Load with User mode privileges
				data = mem_user_read8(addr);

				// Check for Abort
				if (arm.abort_base_restored && (arm.event & 0x40)) {
					break;
				}

				// Writeback
				if (opcode & 0x2000000) {
					offset = shift_ldrstr(opcode);
				} else {
					offset = opcode & 0xfff;
				}
				if (!(opcode & 0x800000)) {
					offset = -offset;
				}
				addr += offset;
				arm.reg[RN] = addr;

				// Check for Abort (before writing Rd)
				if (arm.event & 0x40) {
					break;
				}

				// Write Rd
				LOADREG(RD, data);
				break;

			case 0x60: // STR Rd, [Rn], -reg...
			case 0x68: // STR Rd, [Rn], +reg...
			case 0x70: // STR Rd, [Rn, -reg...]
			case 0x72: // STR Rd, [Rn, -reg...]!
			case 0x78: // STR Rd, [Rn, +reg...]
			case 0x7a: // STR Rd, [Rn, +reg...]!
				if (opcode & 0x10) {
					arm_exception_undefined();
					break;
				}
				// Fall-through
			case 0x40: // STR Rd, [Rn], #-imm
			case 0x48: // STR Rd, [Rn], #+imm
			case 0x50: // STR Rd, [Rn, #-imm]
			case 0x52: // STR Rd, [Rn, #-imm]!
			case 0x58: // STR Rd, [Rn, #+imm]
			case 0x5a: // STR Rd, [Rn, #+imm]!
				addr = GETADDR(RN);

				// Calculate offset
				if (opcode & 0x2000000) {
					offset = shift_ldrstr(opcode);
				} else {
					offset = opcode & 0xfff;
				}
				if (!(opcode & 0x800000)) {
					offset = -offset;
				}

				// Pre-indexed
				if (opcode & 0x1000000) {
					addr += offset;
				}

				// Store
				data = GETREG(RD);
				mem_write32(addr & ~3u, data);

				// Check for Abort
				if (arm.abort_base_restored && (arm.event & 0x40)) {
					break;
				}

				if (!(opcode & 0x1000000)) {
					// Post-indexed
					addr += offset;
					arm.reg[RN] = addr;
				} else if (opcode & 0x200000) {
					// Pre-indexed with writeback
					arm.reg[RN] = addr;
				}
				break;

			case 0x61: // LDR Rd, [Rn], -reg...
			case 0x69: // LDR Rd, [Rn], +reg...
			case 0x71: // LDR Rd, [Rn, -reg...]
			case 0x73: // LDR Rd, [Rn, -reg...]!
			case 0x79: // LDR Rd, [Rn, +reg...]
			case 0x7b: // LDR Rd, [Rn, +reg...]!
				if (opcode & 0x10) {
					arm_exception_undefined();
					break;
				}
				// Fall-through
			case 0x41: // LDR Rd, [Rn], #-imm
			case 0x49: // LDR Rd, [Rn], #+imm
			case 0x51: // LDR Rd, [Rn, #-imm]
			case 0x53: // LDR Rd, [Rn, #-imm]!
			case 0x59: // LDR Rd, [Rn, #+imm]
			case 0x5b: // LDR Rd, [Rn, #+imm]!
				addr = GETADDR(RN);

				// Calculate offset
				if (opcode & 0x2000000) {
					offset = shift_ldrstr(opcode);
				} else {
					offset = opcode & 0xfff;
				}
				if (!(opcode & 0x800000)) {
					offset = -offset;
				}

				// Pre-indexed
				if (opcode & 0x1000000) {
					addr += offset;
				}

				// Load
				data = mem_read32(addr & ~3u);

				// Check for Abort
				if (arm.abort_base_restored && (arm.event & 0x40)) {
					break;
				}

				// Rotate if load is unaligned
				data = arm_ldr_rotate(data, addr);

				if (!(opcode & 0x1000000)) {
					// Post-indexed
					addr += offset;
					arm.reg[RN] = addr;
				} else if (opcode & 0x200000) {
					// Pre-indexed with writeback
					arm.reg[RN] = addr;
				}

				// Check for Abort (before writing Rd)
				if (arm.event & 0x40) {
					break;
				}

				// Write Rd
				LOADREG(RD, data);
				break;

			case 0x64: // STRB Rd, [Rn], -reg...
			case 0x6c: // STRB Rd, [Rn], +reg...
			case 0x74: // STRB Rd, [Rn, -reg...]
			case 0x76: // STRB Rd, [Rn, -reg...]!
			case 0x7c: // STRB Rd, [Rn, +reg...]
			case 0x7e: // STRB Rd, [Rn, +reg...]!
				if (opcode & 0x10) {
					arm_exception_undefined();
					break;
				}
				// Fall-through
			case 0x44: // STRB Rd, [Rn], #-imm
			case 0x4c: // STRB Rd, [Rn], #+imm
			case 0x54: // STRB Rd, [Rn, #-imm]
			case 0x56: // STRB Rd, [Rn, #-imm]!
			case 0x5c: // STRB Rd, [Rn, #+imm]
			case 0x5e: // STRB Rd, [Rn, #+imm]!
				addr = GETADDR(RN);

				// Calculate offset
				if (opcode & 0x2000000) {
					offset = shift_ldrstr(opcode);
				} else {
					offset = opcode & 0xfff;
				}
				if (!(opcode & 0x800000)) {
					offset = -offset;
				}

				// Pre-indexed
				if (opcode & 0x1000000) {
					addr += offset;
				}

				// Store
				data = GETREG(RD);
				mem_write8(addr, data);

				// Check for Abort
				if (arm.abort_base_restored && (arm.event & 0x40)) {
					break;
				}

				if (!(opcode & 0x1000000)) {
					// Post-indexed
					addr += offset;
					arm.reg[RN] = addr;
				} else if (opcode & 0x200000) {
					// Pre-indexed with writeback
					arm.reg[RN] = addr;
				}
				break;

			case 0x65: // LDRB Rd, [Rn], -reg...
			case 0x6d: // LDRB Rd, [Rn], +reg...
			case 0x75: // LDRB Rd, [Rn, -reg...]
			case 0x77: // LDRB Rd, [Rn, -reg...]!
			case 0x7d: // LDRB Rd, [Rn, +reg...]
			case 0x7f: // LDRB Rd, [Rn, +reg...]!
				if (opcode & 0x10) {
					arm_exception_undefined();
					break;
				}
				// Fall-through
			case 0x45: // LDRB Rd, [Rn], #-imm
			case 0x4d: // LDRB Rd, [Rn], #+imm
			case 0x55: // LDRB Rd, [Rn, #-imm]
			case 0x57: // LDRB Rd, [Rn, #-imm]!
			case 0x5d: // LDRB Rd, [Rn, #+imm]
			case 0x5f: // LDRB Rd, [Rn, #+imm]!
				addr = GETADDR(RN);

				// Calculate offset
				if (opcode & 0x2000000) {
					offset = shift_ldrstr(opcode);
				} else {
					offset = opcode & 0xfff;
				}
				if (!(opcode & 0x800000)) {
					offset = -offset;
				}

				// Pre-indexed
				if (opcode & 0x1000000) {
					addr += offset;
				}

				// Load
				data = mem_read8(addr);

				// Check for Abort
				if (arm.abort_base_restored && (arm.event & 0x40)) {
					break;
				}

				if (!(opcode & 0x1000000)) {
					// Post-indexed
					addr += offset;
					arm.reg[RN] = addr;
				} else if (opcode & 0x200000) {
					// Pre-indexed with writeback
					arm.reg[RN] = addr;
				}

				// Check for Abort (before writing Rd)
				if (arm.event & 0x40) {
					break;
				}

				// Write Rd
				LOADREG(RD, data);
				break;

			case 0x80: // STMDA
			case 0x82: // STMDA !
			case 0x90: // STMDB
			case 0x92: // STMDB !
				offset = arm_ldm_stm_offset(opcode);
				addr = arm.reg[RN] - offset;
				writeback = addr;
				if (!(opcode & (1 << 24))) {
					// Decrement After
					addr += 4;
				}
				arm_store_multiple(opcode, addr, writeback);
				break;

			case 0x88: // STMIA
			case 0x8a: // STMIA !
			case 0x98: // STMIB
			case 0x9a: // STMIB !
				offset = arm_ldm_stm_offset(opcode);
				addr = arm.reg[RN];
				writeback = addr + offset;
				if (opcode & (1 << 24)) {
					// Increment Before
					addr += 4;
				}
				arm_store_multiple(opcode, addr, writeback);
				break;

			case 0x84: // STMDA ^
			case 0x86: // STMDA ^!
			case 0x94: // STMDB ^
			case 0x96: // STMDB ^!
				offset = arm_ldm_stm_offset(opcode);
				addr = arm.reg[RN] - offset;
				writeback = addr;
				if (!(opcode & (1 << 24))) {
					// Decrement After
					addr += 4;
				}
				arm_store_multiple_s(opcode, addr, writeback);
				break;

			case 0x8c: // STMIA ^
			case 0x8e: // STMIA ^!
			case 0x9c: // STMIB ^
			case 0x9e: // STMIB ^!
				offset = arm_ldm_stm_offset(opcode);
				addr = arm.reg[RN];
				writeback = addr + offset;
				if (opcode & (1 << 24)) {
					// Increment Before
					addr += 4;
				}
				arm_store_multiple_s(opcode, addr, writeback);
				break;

			case 0x81: // LDMDA
			case 0x83: // LDMDA !
			case 0x91: // LDMDB
			case 0x93: // LDMDB !
				offset = arm_ldm_stm_offset(opcode);
				addr = arm.reg[RN] - offset;
				writeback = addr;
				if (!(opcode & (1 << 24))) {
					// Decrement After
					addr += 4;
				}
				arm_load_multiple(opcode, addr, writeback);
				break;

			case 0x89: // LDMIA
			case 0x8b: // LDMIA !
			case 0x99: // LDMIB
			case 0x9b: // LDMIB !
				offset = arm_ldm_stm_offset(opcode);
				addr = arm.reg[RN];
				writeback = addr + offset;
				if (opcode & (1 << 24)) {
					// Increment Before
					addr += 4;
				}
				arm_load_multiple(opcode, addr, writeback);
				break;

			case 0x85: // LDMDA ^
			case 0x87: // LDMDA ^!
			case 0x95: // LDMDB ^
			case 0x97: // LDMDB ^!
				offset = arm_ldm_stm_offset(opcode);
				addr = arm.reg[RN] - offset;
				writeback = addr;
				if (!(opcode & (1 << 24))) {
					// Decrement After
					addr += 4;
				}
				arm_load_multiple_s(opcode, addr, writeback);
				break;

			case 0x8d: // LDMIA ^
			case 0x8f: // LDMIA ^!
			case 0x9d: // LDMIB ^
			case 0x9f: // LDMIB ^!
				offset = arm_ldm_stm_offset(opcode);
				addr = arm.reg[RN];
				writeback = addr + offset;
				if (opcode & (1 << 24)) {
					// Increment Before
					addr += 4;
				}
				arm_load_multiple_s(opcode, addr, writeback);
				break;

			case 0xa0: case 0xa1: case 0xa2: case 0xa3: // B
			case 0xa4: case 0xa5: case 0xa6: case 0xa7:
			case 0xa8: case 0xa9: case 0xaa: case 0xab:
			case 0xac: case 0xad: case 0xae: case 0xaf:
				// Extract offset bits, and sign-extend
				offset = (opcode << 8);
				offset = (uint32_t) ((int32_t) offset >> 6);
				arm.reg[15] = ((arm.reg[15] + offset + 4) & arm.r15_mask) |
				              (arm.reg[15] & ~arm.r15_mask);
				break;

			case 0xb0: case 0xb1: case 0xb2: case 0xb3: // BL
			case 0xb4: case 0xb5: case 0xb6: case 0xb7:
			case 0xb8: case 0xb9: case 0xba: case 0xbb:
			case 0xbc: case 0xbd: case 0xbe: case 0xbf:
				// Extract offset bits, and sign-extend
				offset = (opcode << 8);
				offset = (uint32_t) ((int32_t) offset >> 6);
				arm.reg[14] = arm.reg[15] - 4;
				arm.reg[15] = ((arm.reg[15] + offset + 4) & arm.r15_mask) |
				              (arm.reg[15] & ~arm.r15_mask);
				break;

			case 0xc0: case 0xc1: case 0xc2: case 0xc3: // Co-pro
			case 0xc4: case 0xc5: case 0xc6: case 0xc7:
			case 0xc8: case 0xc9: case 0xca: case 0xcb:
			case 0xcc: case 0xcd: case 0xce: case 0xcf:
			case 0xd0: case 0xd1: case 0xd2: case 0xd3:
			case 0xd4: case 0xd5: case 0xd6: case 0xd7:
			case 0xd8: case 0xd9: case 0xda: case 0xdb:
			case 0xdc: case 0xdd: case 0xde: case 0xdf:
#ifdef FPA
				if ((opcode & 0xf00) == 0x100 || (opcode & 0xf00) == 0x200) {
					fpaopcode(opcode);
					break;
				}
#endif
				arm_exception_undefined();
				break;

			case 0xe0: case 0xe2: case 0xe4: case 0xe6: // MCR
			case 0xe8: case 0xea: case 0xec: case 0xee:
#ifdef FPA
				if ((opcode & 0xf00) == 0x100) {
					fpaopcode(opcode);
					break;
				}
#endif
				if ((opcode & 0xf10) == 0xf10 && ARM_MODE_PRIV(arm.mode)) {
					cp15_write(opcode, arm.reg[RD]);
				} else {
					arm_exception_undefined();
				}
				break;

			case 0xe1: case 0xe3: case 0xe5: case 0xe7: // MRC
			case 0xe9: case 0xeb: case 0xed: case 0xef:
#ifdef FPA
				if ((opcode & 0xf00) == 0x100) {
					fpaopcode(opcode);
					break;
				}
#endif
				if ((opcode & 0xf10) == 0xf10 && ARM_MODE_PRIV(arm.mode)) {
					if (RD == 15) {
						arm.reg[RD] = (arm.reg[RD] & arm.r15_mask) |
						              (cp15_read(opcode) & ~arm.r15_mask);
					} else {
						arm.reg[RD] = cp15_read(opcode);
					}
				} else {
					arm_exception_undefined();
				}
				break;

			case 0xf0: case 0xf1: case 0xf2: case 0xf3: // SWI
			case 0xf4: case 0xf5: case 0xf6: case 0xf7:
			case 0xf8: case 0xf9: case 0xfa: case 0xfb:
			case 0xfc: case 0xfd: case 0xfe: case 0xff:
				opSWI(opcode);
				break;

			default:
				if (arm.arch_v4) {
					arm_exception_undefined();
				} else {
					arm_unpredictable(opcode);
				}
				break;
			}
		}

	// This label is used to skip the switch above
skip:

		if (arm.event != 0) {
			if (!ARM_MODE_32(arm.mode)) {
				arm.reg[16] &= ~0xc0u;
				arm.reg[16] |= ((arm.reg[15] & 0xc000000) >> 20);
			}

			if (arm.event & 0x40) {
				// Data Abort
				exception(ABORT, 0x14, 0);
				arm.event &= ~0x40u;
			} else if ((arm.event & 2) && !(arm.reg[16] & 0x40)) {
				// FIQ
				exception(FIQ, 0x20, 0);
			} else if ((arm.event & 1) && !(arm.reg[16] & 0x80)) {
				// IRQ
				exception(IRQ, 0x1c, 0);
			}
		}

		arm.reg[15] += 4;
	}
	inscount += 200;

	return 200;
}
