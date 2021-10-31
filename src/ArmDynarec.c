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

int blockend;

/*Preliminary FPA emulation. This works to an extent - !Draw works with it, !SICK
  seems to (FPA Whetstone scores are around 100x without), but !AMPlayer doesn't
  work, and GCC stuff tends to crash.*/
//#define FPA

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#if defined __linux__ || defined __MACH__
#	include <unistd.h>
#	include <sys/mman.h>
#elif defined WIN32 || defined _WIN32
#	include <Windows.h>
#endif

#include "rpcemu.h"
#include "arm.h"
#include "cp15.h"
#include "mem.h"

#if defined __amd64__
#	include "codegen_amd64.h"
#elif defined i386 || defined __i386 || defined __i386__ || defined _X86_
#	include "codegen_x86.h"
#else
#	error "Fatal error : no recompiler available for this architecture"
#endif

ARMState arm;

uint32_t inscount;
int cpsr;
uint32_t *pcpsr;

uint8_t flaglookup[16][16];

uint32_t *usrregs[16];
int prog32;

static int unpredictable_count = 1000; ///< Limit logging of unpredictable instructions

#define NFSET	((arm.reg[cpsr] & NFLAG) ? 1u : 0)
#define ZFSET	((arm.reg[cpsr] & ZFLAG) ? 1u : 0)
#define CFSET	((arm.reg[cpsr] & CFLAG) ? 1u : 0)
#define VFSET	((arm.reg[cpsr] & VFLAG) ? 1u : 0)

#define refillpipeline() blockend=1;

#include "arm_common.h"

uint32_t pccache;
static const uint32_t *pccache2;

/**
 * Return true if this ARM core is the dynarec version
 *
 * @return 1 yes this is dynarec
 */
int
arm_is_dynarec(void)
{
	return 1;
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
	refillpipeline();
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

#if defined __linux__ || defined __MACH__
/**
 * Grant executable privilege to a region of memory (Unix)
 *
 * @param ptr Pointer to region of memory
 * @param len Length of region of memory
 */
void
set_memory_executable(void *ptr, size_t len)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	const long page_mask = ~(page_size - 1);
	void *start, *addr;
	long end;
    int mmap_flags = 0;

	start = (void *) ((long) ptr & page_mask);
	end = ((long) ptr + len + page_size - 1) & page_mask;
	len = (size_t) (end - (long) start);
    
#if __APPLE__
    // More up-to-date versions of OS X require "mmap" to be called prior to "mprotect".
    // Certain versions also require the MAP_JIT flag as well.
    // Try without first, and if that fails, add the flag in.
    mmap_flags = MAP_PRIVATE | MAP_ANON | MAP_FIXED;

    addr = mmap(NULL, page_size, PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (addr == MAP_FAILED)
    {
        mmap_flags |= MAP_JIT;
    }
    else
    {
        munmap(addr, page_size);
    }

    addr = mmap(start, len, PROT_READ | PROT_WRITE | PROT_EXEC, mmap_flags, -1, 0);

    if (addr == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

#endif

	if (mprotect(start, len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
		perror("mprotect");
		exit(1);
	}
}

#elif defined WIN32 || defined _WIN32
/**
 * Grant executable privilege to a region of memory (Windows)
 *
 * @param ptr Pointer to region of memory
 * @param len Length of region of memory
 */
void
set_memory_executable(void *ptr, size_t len)
{
	DWORD old_protect;

	if (!VirtualProtect(ptr, len, PAGE_EXECUTE_READWRITE, &old_protect)) {
		fprintf(stderr, "VirtualProtect() failed: error code 0x%lx\n", GetLastError());
		exit(1);
	}
}

#else
/**
 * Stub implementation for when another implementation does not apply.
 */
void
set_memory_executable(void *ptr, size_t len)
{
	NOT_USED(ptr);
	NOT_USED(len);
}
#endif

#include "ArmDynarecOps.h"

static const OpFn opcodes[256] = {
	opANDreg,  opANDregS, opEORreg,  opEORregS, opSUBreg,  opSUBregS, opRSBreg,  opRSBregS, // 00
	opADDreg,  opADDregS, opADCreg,  opADCregS, opSBCreg,  opSBCregS, opRSCreg,  opRSCregS, // 08
	opSWPword, opTSTreg,  opMSRcreg, opTEQreg,  opSWPbyte, opCMPreg,  opMSRsreg, opCMNreg,  // 10
	opORRreg,  opORRregS, opMOVreg,  opMOVregS, opBICreg,  opBICregS, opMVNreg,  opMVNregS, // 18

	opANDimm,  opANDimmS, opEORimm,  opEORimmS, opSUBimm,  opSUBimmS, opRSBimm,  opRSBimmS, // 20
	opADDimm,  opADDimmS, opADCimm,  opADCimmS, opSBCimm,  opSBCimmS, opRSCimm,  opRSCimmS, // 28
	opUNALLOC, opTSTimm,  opMSRcimm, opTEQimm,  opUNALLOC, opCMPimm,  opMSRsimm, opCMNimm,  // 30
	opORRimm,  opORRimmS, opMOVimm,  opMOVimmS, opBICimm,  opBICimmS, opMVNimm,  opMVNimmS, // 38

	opSTR,    opLDR,    opSTRT,   opLDRT,   opSTRB,   opLDRB,   opSTRBT,  opLDRBT,   // 40
	opSTR,    opLDR,    opSTRT,   opLDRT,   opSTRB,   opLDRB,   opSTRBT,  opLDRBT,   // 48
	opSTR,    opLDR,    opSTR,    opLDR,    opSTRB,   opLDRB,   opSTRB,   opLDRB,    // 50
	opSTR,    opLDR,    opSTR,    opLDR,    opSTRB,   opLDRB,   opSTRB,   opLDRB,    // 58

	opSTR,    opLDR,    opSTRT,   opLDRT,   opSTRB,   opLDRB,   opSTRBT,  opLDRBT,   // 60
	opSTR,    opLDR,    opSTRT,   opLDRT,   opSTRB,   opLDRB,   opSTRBT,  opLDRBT,   // 68
	opSTR,    opLDR,    opSTR,    opLDR,    opSTRB,   opLDRB,   opSTRB,   opLDRB,    // 70
	opSTR,    opLDR,    opSTR,    opLDR,    opSTRB,   opLDRB,   opSTRB,   opLDRB,    // 78

	opSTMD,   opLDMD,   opSTMD,   opLDMD,   opSTMDS,  opLDMDS,  opSTMDS,  opLDMDS,   // 80
	opSTMI,   opLDMI,   opSTMI,   opLDMI,   opSTMIS,  opLDMIS,  opSTMIS,  opLDMIS,   // 88
	opSTMD,   opLDMD,   opSTMD,   opLDMD,   opSTMDS,  opLDMDS,  opSTMDS,  opLDMDS,   // 90
	opSTMI,   opLDMI,   opSTMI,   opLDMI,   opSTMIS,  opLDMIS,  opSTMIS,  opLDMIS,   // 98

	opB,      opB,      opB,      opB,      opB,      opB,      opB,      opB,       // a0
	opB,      opB,      opB,      opB,      opB,      opB,      opB,      opB,       // a8
	opBL,     opBL,     opBL,     opBL,     opBL,     opBL,     opBL,     opBL,      // b0
	opBL,     opBL,     opBL,     opBL,     opBL,     opBL,     opBL,     opBL,      // b8

	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   // c0
	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   // c8
	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   // d0
	opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,  opcopro,   // d8

	opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,     // e0
	opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,    opMCR,    opMRC,     // e8
	opSWI,    opSWI,    opSWI,    opSWI,    opSWI,    opSWI,    opSWI,    opSWI,     // f0
	opSWI,    opSWI,    opSWI,    opSWI,    opSWI,    opSWI,    opSWI,    opSWI      // f8
};

int linecyc=0;

static inline int
arm_opcode_needs_pc(uint32_t opcode)
{
	// Is this a load, store, branch, co-pro or SWI?
	if (opcode & 0xc000000) {
		return 1;
	}
	// Is this a swap, status register transfer, or unallocated instruction?
	if ((opcode & 0xd900000) == 0x1000000) {
		return 1;
	}
	// Is this a data-processing operation that uses PC?
	if (RN == 15 || RD == 15 || ((opcode & 0x2000000) == 0 && (RM == 15))) {
		return 1;
	}
	// Is this a load/store extension?
	if (arm.arch_v4 && (((opcode & 0xe0000f0) == 0xb0) || ((opcode & 0xe1000d0) == 0x1000d0))) {
		return 1;
	}
	return 0;
}

static inline int
arm_opcode_may_abort(uint32_t opcode)
{
	/* Is this a single or multiple data transfer? */
	if (((opcode + 0x6000000) & 0xf000000) >= 0xa000000) {
		return 1;
	}
	/* Is this a swap? */
	if ((opcode & 0x0fb000f0) == 0x01000090) {
		return 1;
	}
	// Is this a load/store extension?
	if (arm.arch_v4) {
		if (((opcode & 0xe0000f0) == 0xb0) || ((opcode & 0xe1000d0) == 0x1000d0)) {
			return 1;
		}
	}

	return 0;
}

/**
 * Select and return pointer to opcode function.
 *
 * @param opcode Opcode
 * @return Pointer to function
 */
static inline OpFn
arm_opcode_fn(uint32_t opcode)
{
	if (arm.arch_v4) {
		if ((opcode & 0xe0000f0) == 0xb0) {
			// LDRH/STRH
			if (opcode & 0x100000) {
				return opLDRH;
			} else {
				return opSTRH;
			}
		} else if ((opcode & 0xe1000d0) == 0x1000d0) {
			// LDRSB/LDRSH
			if ((opcode & 0xf0) == 0xd0) {
				return opLDRSB;
			} else {
				return opLDRSH;
			}
		}
	}

	return opcodes[(opcode >> 20) & 0xff];
}

/**
 * Execute several ARM instructions.
 *
 * @return A hint roughly proportional to the amount of instructions executed.
 */
int
arm_exec(void)
{
	for (linecyc = 256; linecyc >= 0; linecyc--) {
		if (!isblockvalid(PC)) {
			// Interpret block
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
			blockend = 0;
			do {
				const uint32_t opcode = pccache2[PC >> 2];

				if ((opcode & 0x0e000000) == 0x0a000000) { blockend = 1; } /* Always end block on branches */
				if ((opcode & 0x0c000000) == 0x0c000000) { blockend = 1; } /* And SWIs and copro stuff */
				if (!(opcode & 0x0c000000) && (RD == 15)) { blockend = 1; } /* End if R15 can be modified */
				if ((opcode & 0x0e108000) == 0x08108000) { blockend = 1; } /* End if R15 reloaded from LDM */
				if ((opcode & 0x0c100000) == 0x04100000 && (RD == 15)) { blockend = 1; } /* End if R15 reloaded from LDR */
				if (flaglookup[opcode >> 28][(*pcpsr) >> 28]) {
					OpFn fn = arm_opcode_fn(opcode);
					fn(opcode);
				}
				// if ((opcode & 0x0e000000) == 0x0a000000) blockend = 1; /* Always end block on branches */
				// if ((opcode & 0x0c000000) == 0x0c000000) blockend = 1; /* And SWIs and copro stuff */
				arm.reg[15] += 4;
				if ((PC & 0xffc) == 0) {
					blockend = 1;
				}
				inscount++;
			} while (!blockend && !(arm.event & 0x40));
		} else {
			const uint32_t hash = HASH(PC);
			/* if (pagedirty[PC>>9])
			{
				pagedirty[PC>>9]=0;
				cacheclearpage(PC>>9);
			}
			else */ if (codeblockpc[hash] == PC) {
				const uint32_t templ = codeblocknum[hash];
				void (*gen_func)(void);

				gen_func = (void *) (&rcodeblock[templ][BLOCKSTART]);
				// gen_func=(void *)(&codeblock[blocks[templ]>>24][blocks[templ]&0xFFF][4]);
				gen_func();
				if (arm.event & 0x40) {
					arm.reg[15] += 4;
				}
				if ((arm.reg[cpsr] & arm.mmask) != arm.mode) {
					updatemode(arm.reg[cpsr] & arm.mmask);
				}
			} else {
				uint32_t opcode;

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
				initcodeblock(PC);
				blockend = 0;
				do {
					opcode = pccache2[PC >> 2];
					if ((opcode >> 28) == 0xf) {
						// NV condition code
						generatepcinc();
					} else {
						if (arm_opcode_needs_pc(opcode)) {
							generateupdatepc();
						}
						generatepcinc();
						if ((opcode & 0x0e000000) == 0x0a000000) {
							generateupdateinscount();
						}
						if ((opcode >> 28) != 0xe) {
							generateflagtestandbranch(opcode, pcpsr);//,flaglookup);
						} else {
							lastflagchange = 0;
						}
						generatecall(arm_opcode_fn(opcode), opcode, pcpsr);
						if (arm_opcode_may_abort(opcode)) {
							generateirqtest();
						}
						// if ((opcode & 0x0e000000) == 0x0a000000) blockend = 1; /* Always end block on branches */
						if ((opcode & 0x0c000000) == 0x0c000000) blockend = 1; /* And SWIs and copro stuff */
						if (!(opcode & 0x0c000000) && (RD == 15)) blockend = 1; /* End if R15 can be modified */
						if ((opcode & 0x0e108000) == 0x08108000) blockend = 1; /* End if R15 reloaded from LDM */
						if ((opcode & 0x0c100000) == 0x04100000 && (RD == 15)) blockend=1; /* End if R15 reloaded from LDR */
						if (flaglookup[opcode >> 28][(*pcpsr) >> 28]) {
							OpFn fn = arm_opcode_fn(opcode);
							fn(opcode);
						}
					}
					arm.reg[15] += 4;
					if ((PC & 0xffc) == 0) {
						blockend = 1;
					}
				} while (!blockend && !(arm.event & 0x40));
				endblock(opcode);
			}
		}

		if (arm.event != 0) {
			if (!ARM_MODE_32(arm.mode)) {
				arm.reg[16] &= ~0xc0u;
				arm.reg[16] |= ((arm.reg[15] & 0xc000000) >> 20);
			}

			if (arm.event & 0x40) {
				// Data Abort
				arm.reg[15] -= 4;
				exception(ABORT, 0x14, 0);
				arm.reg[15] += 4;
				arm.event &= ~0x40u;
			} else if ((arm.event & 2) && !(arm.reg[16] & 0x40)) {
				// FIQ
				arm.reg[15] -= 4;
				exception(FIQ, 0x20, 0);
				arm.reg[15] += 4;
			} else if ((arm.event & 1) && !(arm.reg[16] & 0x80)) {
				// IRQ
				arm.reg[15] -= 4;
				exception(IRQ, 0x1c, 0);
				arm.reg[15] += 4;
			}
		}
	}

	return 1000;
}
