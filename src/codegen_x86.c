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

//ESI is pointer to ARMState

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "rpcemu.h"
#include "arm.h"
#include "arm_common.h"
#include "codegen_x86.h"
#include "mem.h"

int lastflagchange;

uint8_t rcodeblock[BLOCKS][1792+512+64] __attribute__ ((aligned (4096)));
static const void *codeblockaddr[BLOCKS];
uint32_t codeblockpc[0x8000];
int codeblocknum[0x8000];
static uint8_t codeblockpresent[0x10000];

//#define BLOCKS 4096
//#define HASH(l) ((l>>3)&0x3fff)

static int blocknum;//,blockcount;
static int tempinscount;

static int codeblockpos;

static uint8_t lahf_table_add[256];
static uint8_t lahf_table_sub[256];

static int blockpoint, blockpoint2;
static uint32_t blocks[BLOCKS];
static int pcinc;
static int lastrecompiled;
static int block_enter;

static uint32_t currentblockpc, currentblockpc2;

static inline void
addbyte(uint32_t a)
{
	rcodeblock[blockpoint2][codeblockpos] = (uint8_t) a;
	codeblockpos++;
}

static inline void
addlong(uint32_t a)
{
	memcpy(&rcodeblock[blockpoint2][codeblockpos], &a, sizeof(uint32_t));
	codeblockpos += 4;
}

static inline void
addptr(const void *a)
{
	addlong((uint32_t) a);
}

#include "codegen_x86_common.h"

#define gen_x86_pop_reg(x86reg)		addbyte(0x58 | x86reg)
#define gen_x86_push_reg(x86reg)	addbyte(0x50 | x86reg)

static inline void
gen_x86_mov_reg32_stack(int x86reg, int offset)
{
	addbyte(0x89);
	if (offset != 0) {
		addbyte(0x44 | (x86reg << 3)); addbyte(0x24); addbyte(offset);
	} else {
		addbyte(0x04 | (x86reg << 3)); addbyte(0x24);
	}
}

static inline void
gen_x86_mov_stack_reg32(int x86reg, int offset)
{
	addbyte(0x8b);
	if (offset != 0) {
		addbyte(0x44 | (x86reg << 3)); addbyte(0x24); addbyte(offset);
	} else {
		addbyte(0x04 | (x86reg << 3)); addbyte(0x24);
	}
}

void
initcodeblocks(void)
{
	int c;

	// Clear all blocks
	memset(codeblockpc, 0xff, sizeof(codeblockpc));
	memset(blocks, 0xff, sizeof(blocks));
	for (c = 0; c < BLOCKS; c++) {
		codeblockaddr[c] = &rcodeblock[c][0];
	}
	blockpoint = 0;

	for (c = 0; c < 256; c++) {
		lahf_table_add[c] = 0;
		if (c & 1) {
			lahf_table_add[c] |= 0x20; // C flag
		}
		lahf_table_add[c] |= (c & 0xc0); // N and Z flags
		lahf_table_sub[c] = lahf_table_add[c] ^ 0x20;
	}

	// Set memory pages containing rcodeblock[]s executable -
	// necessary when NX/XD feature is active on CPU(s)
	set_memory_executable(rcodeblock, sizeof(rcodeblock));
}

void
resetcodeblocks(void)
{
	int c;

	blockpoint = 0;

	for (c = 0; c < BLOCKS; c++) {
		if (blocks[c] != 0xffffffff) {
			codeblockpc[blocks[c] & 0x7fff] = 0xffffffff;
			codeblocknum[blocks[c] & 0x7fff] = 0xffffffff;
			blocks[c] = 0xffffffff;
		}
	}
}

void
cacheclearpage(uint32_t a)
{
	int c, d;

	if (!codeblockpresent[a & 0xffff]) {
		return;
	}
	codeblockpresent[a & 0xffff] = 0;
	// a >>= 10;
	d = HASH(a << 12);
	for (c = 0; c < 0x400; c++) {
		if ((codeblockpc[c + d] >> 12) == a) {
			codeblockpc[c + d] = 0xffffffff;
		}
	}
}

void
initcodeblock(uint32_t l)
{
	codeblockpresent[(l >> 12) & 0xffff] = 1;
	tempinscount = 0;
	// rpclog("Initcodeblock %08x\n", l);
	blockpoint++;
	blockpoint &= (BLOCKS - 1);
	if (blocks[blockpoint] != 0xffffffff) {
		// rpclog("Chucking out block %08x %d %03x\n", blocks[blockpoint], blocks[blockpoint] >> 24, blocks[blockpoint] & 0xfff);
		codeblockpc[blocks[blockpoint] & 0x7fff] = 0xffffffff;
		codeblocknum[blocks[blockpoint] & 0x7fff] = 0xffffffff;
	}
	blocknum = HASH(l);
//        blockcount=0;//codeblockcount[blocknum];
//        codeblockcount[blocknum]++;
//        if (codeblockcount[blocknum]==3) codeblockcount[blocknum]=0;
	codeblockpos = 0;
	codeblockpc[blocknum] = l;
	codeblocknum[blocknum] = blockpoint;
	blocks[blockpoint] = blocknum;
	blockpoint2 = blockpoint;

	// Block Epilogue
	addbyte(0x83); addbyte(0xc4); addbyte(12); // ADD $12,%esp
	// Restore registers
	gen_x86_pop_reg(EBX);
	gen_x86_pop_reg(ESI);
	gen_x86_pop_reg(EDI);
	gen_x86_leave();
	gen_x86_ret();

	addbyte(0xe9); addlong(0); // JMP end - don't know where end is yet - see endblock()

	// Block Prologue
	assert(codeblockpos <= BLOCKSTART);
	codeblockpos = BLOCKSTART;
	// Set up a stack frame and preserve registers that are callee-saved
	gen_x86_push_reg(EBP);
	addbyte(0x89); addbyte(0xe5); // MOV %esp,%ebp
	gen_x86_push_reg(EDI);
	gen_x86_push_reg(ESI);
	gen_x86_push_reg(EBX);
	// Align stack to a multiple of 16 bytes - required by Mac OS X
	addbyte(0x83); addbyte(0xec); addbyte(12); // SUB $12,%esp

	addbyte(0xbe); addptr(&arm); // MOV $(&arm),%esi
	block_enter = codeblockpos;

	currentblockpc = arm.reg[15] & arm.r15_mask;
	currentblockpc2 = PC;
}

static const int recompileinstructions[256] = {
	1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0, // 00
	0,1,0,1,0,1,0,0,1,1,1,1,1,1,1,1, // 10
	1,1,1,1,1,1,0,0,1,1,0,0,0,0,0,0, // 20
	0,1,0,1,0,1,0,0,1,1,1,1,1,1,1,1, // 30

	1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, // 40
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 50
	1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, // 60
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 70

	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 80
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 90
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // a0
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // b0

	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // c0
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // d0
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // e0
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // f0
};

static void
gen_load_reg(int reg, int x86reg)
{
	if (reg != 0) {
		addbyte(0x8b); addbyte(0x46 | (x86reg << 3)); addbyte(reg<<2);
	} else {
		addbyte(0x8b); addbyte(0x06 | (x86reg << 3));
	}
}

static void
gen_save_reg(int reg, int x86reg)
{
	if (reg != 0) {
		addbyte(0x89); addbyte(0x46 | (x86reg << 3)); addbyte(reg<<2);
	} else {
		addbyte(0x89); addbyte(0x06 | (x86reg << 3));
	}
}

static int
generate_shift(uint32_t opcode)
{
	uint32_t shift_amount;

	if (opcode & 0x10) {
		// Can't do register shifts or multiplies
		return 0;
	}
	if ((opcode & 0xff0) == 0) {
		// No shift
		gen_load_reg(RM, EAX);
		return 1;
	}
	shift_amount = (opcode >> 7) & 0x1f;
	switch (opcode & 0x60) {
	case 0x00: // LSL
		gen_load_reg(RM, EAX);
		if (shift_amount != 0) {
			addbyte(0xc1); addbyte(0xe0); addbyte(shift_amount); // SHL $shift_amount,%eax
		}
		return 1;
	case 0x20: // LSR
		if (shift_amount != 0) {
			gen_load_reg(RM, EAX);
			addbyte(0xc1); addbyte(0xe8); addbyte(shift_amount); // SHR $shift_amount,%eax
		} else {
			addbyte(0x31); addbyte(0xc0); // XOR %eax,%eax
		}
		return 1;
	case 0x40: // ASR
		if (shift_amount == 0) {
			shift_amount = 31;
		}
		gen_load_reg(RM, EAX);
		addbyte(0xc1); addbyte(0xf8); addbyte(shift_amount); // SAR $shift_amount,%eax
		return 1;
	default: // ROR
		if (shift_amount == 0) {
			// RRX
			break;
		}
		gen_load_reg(RM, EAX);
		addbyte(0xc1); addbyte(0xc8); addbyte(shift_amount); // ROR $shift_amount,%eax
		return 1;
	}
	return 0;
}

static int
generateshiftflags(uint32_t opcode, uint32_t *pcpsr)
{
	uint32_t shift_amount;

	if (opcode & 0x10) {
		// Can't do register shifts or multiplies
		return 0;
	}
	if ((opcode & 0xff0) == 0) {
		// No shift
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		gen_load_reg(RM, EAX);
		addbyte(0x81); addbyte(0xe1); addlong(0x3fffffff); // AND $~(NFLAG|ZFLAG),%ecx
		return 1;
	}

	shift_amount = (opcode >> 7) & 0x1f;
	switch (opcode & 0x60) {
	case 0x00: // LSL
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		gen_load_reg(RM, EAX);
		if (shift_amount != 0) {
			addbyte(0x81); addbyte(0xe1); addlong(0x1fffffff); // AND $~(NFLAG|ZFLAG|CFLAG),%ecx
			addbyte(0xc1); addbyte(0xe0); addbyte(shift_amount); // SHL $shift_amount,%eax
			addbyte(0x73); addbyte(6); // JNC nocarry
			addbyte(0x81); addbyte(0xc9); addlong(CFLAG); // OR $CFLAG,%ecx
		} else {
			addbyte(0x81); addbyte(0xe1); addlong(0x3fffffff); // AND $~(NFLAG|ZFLAG),%ecx
		}
		return 1;
	case 0x20: // LSR
		if (shift_amount != 0) {
			addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
			addbyte(0x81); addbyte(0xe1); addlong(0x1fffffff); // AND $~(NFLAG|ZFLAG|CFLAG),%ecx
			gen_load_reg(RM, EAX);
			addbyte(0xc1); addbyte(0xe8); addbyte(shift_amount); // SHR $shift_amount,%eax
			addbyte(0x73); addbyte(6); // JNC nocarry
			addbyte(0x81); addbyte(0xc9); addlong(CFLAG); // OR $CFLAG,%ecx
		} else {
			return 0;
			addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
			addbyte(0x81); addbyte(0xe1); addlong(0x1fffffff); // AND $~(NFLAG|ZFLAG|CFLAG),%ecx
			addbyte(0xa9); addlong(0x80000000); // TEST $0x80000000,%eax
			addbyte(0x74); addbyte(6); // JZ nocarry
			addbyte(0x81); addbyte(0xc9); addlong(CFLAG); // OR $CFLAG,%ecx
			addbyte(0x31); addbyte(0xc0); // XOR %eax,%eax
		}
		return 1;
	case 0x40: // ASR
		return 0;
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		addbyte(0x81); addbyte(0xe1); addlong(0x1fffffff); // AND $~(NFLAG|ZFLAG|CFLAG),%ecx
		if (shift_amount == 0) {
			gen_load_reg(RM, EAX);
			addbyte(0xa9); addlong(0x80000000); // TEST $0x80000000,%eax
			addbyte(0x74); addbyte(6); // JZ nocarry
			addbyte(0x81); addbyte(0xc9); addlong(CFLAG); // OR $CFLAG,%ecx
			addbyte(0xc1); addbyte(0xf8); addbyte(31); // SAR $31,%eax
		} else {
			gen_load_reg(RM, EAX);
			addbyte(0xc1); addbyte(0xf8); addbyte(shift_amount); // SAR $shift_amount,%eax
			addbyte(0x73); addbyte(6); // JNC nocarry
			addbyte(0x81); addbyte(0xc9); addlong(CFLAG); // OR $CFLAG,%ecx
		}
		return 1;
	default: // ROR
		return 0;
		if (shift_amount == 0) {
			// RRX
			break;
		}
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		gen_load_reg(RM, EAX);
		addbyte(0x81); addbyte(0xe1); addlong(0x1fffffff); // AND $~(NFLAG|ZFLAG|CFLAG),%ecx
		addbyte(0xc1); addbyte(0xc8); addbyte(shift_amount); // ROR $shift_amount,%eax
		addbyte(0x73); addbyte(6); // JNC nocarry
		addbyte(0x81); addbyte(0xc9); addlong(CFLAG); // OR $CFLAG,%ecx
		return 1;
	}
	return 0;
}

static uint32_t
gen_imm_cflag(uint32_t opcode, uint32_t *pcpsr)
{
	const uint32_t imm = arm_imm(opcode);

	addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
	if (opcode & 0xf00) {
		if (imm & 0x80000000) {
			addbyte(0x81); addbyte(0xc9); addlong(CFLAG); // OR $CFLAG,%ecx
		} else {
			addbyte(0x81); addbyte(0xe1); addlong(0x1fffffff); // AND $~(NFLAG|ZFLAG|CFLAG),%ecx
		}
	}
	if (!(opcode & 0xf00) || (imm & 0x80000000)) {
		addbyte(0x81); addbyte(0xe1); addlong(0x3fffffff); // AND $~(NFLAG|ZFLAG),%ecx
	}
	return imm;
}

static void
gen_data_proc_reg(uint32_t opcode, uint8_t op, int dirmatters)
{
	if (dirmatters || RN == 15) {
		gen_load_reg(RN, EDX);
		if (RN == 15) {
			addbyte(0x81); addbyte(0xe2); addlong(arm.r15_mask); // AND $arm.r15_mask,%edx
		}
		addbyte(0x01|op); addbyte(0xc2); // OP %eax,%edx
		gen_save_reg(RD, EDX);
	} else {
		addbyte(0x03|op); addbyte(0x46); addbyte(RN<<2); // OP RN,%eax
		gen_save_reg(RD, EAX);
	}
}

static void
gen_data_proc_imm(uint32_t opcode, uint8_t op, uint32_t imm)
{
	if (RN == RD) {
		// Can use RMW instruction
		addbyte(0x81); addbyte(0x46|op); addbyte(RD<<2); addlong(imm); // OPL $imm,RD
	} else {
		// Load/modify/store
		gen_load_reg(RN, EAX);
		if (RN == 15 && arm.r15_mask != 0xfffffffc) {
			addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
		}
		if (!(imm & ~0x7f)) {
			addbyte(0x83); addbyte(0xc0|op); addbyte(imm); // OP $imm,%eax
		} else {
			addbyte(0x81); addbyte(0xc0|op); addlong(imm); // OP $imm,%eax
		}
		gen_save_reg(RD, EAX);
	}
}

static void
gen_flags_add(uint32_t *pcpsr)
{
	int jump_not_overflow;

	gen_x86_lahf();
	addbyte(0x0f); addbyte(0xb6); addbyte(0xc4); // MOVZBL %ah,%eax
	jump_not_overflow = gen_x86_jump_forward(CC_NO);
	addbyte(0x81); addbyte(0xc9); addlong(VFLAG); // OR $VFLAG,%ecx
	// .not_overflow
	gen_x86_jump_here(jump_not_overflow);
	addbyte(0x0f); addbyte(0xb6); addbyte(0x80); addptr(lahf_table_add); // MOVZBL lahf_table_add(%eax),%eax
	addbyte(0xc1); addbyte(0xe0); addbyte(24); // SHL $24,%eax
	addbyte(0x09); addbyte(0xc1); // OR %eax,%ecx
	addbyte(0x89); addbyte(0x0d); addptr(pcpsr); // MOV %ecx,pcpsr
}

static void
gen_flags_sub(uint32_t *pcpsr)
{
	int jump_not_overflow;

	gen_x86_lahf();
	addbyte(0x0f); addbyte(0xb6); addbyte(0xc4); // MOVZBL %ah,%eax
	jump_not_overflow = gen_x86_jump_forward(CC_NO);
	addbyte(0x81); addbyte(0xc9); addlong(VFLAG); // OR $VFLAG,%ecx
	// .not_overflow
	gen_x86_jump_here(jump_not_overflow);
	addbyte(0x0f); addbyte(0xb6); addbyte(0x80); addptr(lahf_table_sub); // MOVZBL lahf_table_sub(%eax),%eax
	addbyte(0xc1); addbyte(0xe0); addbyte(24); // SHL $24,%eax
	addbyte(0x09); addbyte(0xc1); // OR %eax,%ecx
	addbyte(0x89); addbyte(0x0d); addptr(pcpsr); // MOV %ecx,pcpsr
}

static void
gen_flags_logical(uint32_t *pcpsr)
{
	gen_x86_lahf();
	addbyte(0x80); addbyte(0xe4); addbyte(0xc0); // AND $(NFLAG|ZFLAG),%ah
	addbyte(0x0f); addbyte(0xb6); addbyte(0xc4); // MOVZBL %ah,%eax
	addbyte(0xc1); addbyte(0xe0); addbyte(24); // SHL $24,%eax
	addbyte(0x09); addbyte(0xc1); // OR %eax,%ecx
	addbyte(0x89); addbyte(0x0d); addptr(pcpsr); // MOV %ecx,pcpsr
}

static void
gen_flags_long_multiply(uint32_t *pcpsr)
{
	int jump_not_zero;

	addbyte(0x09); addbyte(0xd0); // OR %edx,%eax
	jump_not_zero = gen_x86_jump_forward(CC_NZ);
	addbyte(0x81); addbyte(0xc9); addlong(ZFLAG); // OR $ZFLAG,%ecx
	// .not_zero
	gen_x86_jump_here(jump_not_zero);
	addbyte(0x81); addbyte(0xe2); addlong(NFLAG); // AND $NFLAG,%edx
	addbyte(0x09); addbyte(0xd1); // OR %edx,%ecx
	addbyte(0x89); addbyte(0x0d); addptr(pcpsr); // MOV %ecx,pcpsr
}

static void
gen_test_armirq(void)
{
	addbyte(0xf7); addbyte(0x46); addbyte(offsetof(ARMState, event)); addlong(0x40); // TESTL $0x40,arm.event
	gen_x86_jump(CC_NZ, 0);
}

/**
 * Register usage:
 *	%ebx	addr
 *	%eax	data (out)
 */
static void
genldr(void)
{
	int jump_nextbit, jump_notinbuffer;

	addbyte(0x89); addbyte(0xda); // MOV %ebx,%edx
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xea); addbyte(12); // SHR $12,%edx
	addbyte(0x83); addbyte(0xe0); addbyte(0xfc); // AND $0xfffffffc,%eax
	addbyte(0x8b); addbyte(0x14); addbyte(0x95); addptr(vraddrl); // MOV vraddrl(,%edx,4),%edx
	addbyte(0xf6); addbyte(0xc2); addbyte(1); // TEST $1,%dl
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x8b); addbyte(0x04); addbyte(0x02); // MOV (%edx,%eax),%eax
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	// .notinbuffer
	gen_x86_jump_here(jump_notinbuffer);
	gen_x86_mov_reg32_stack(EAX, 0);
	gen_x86_call(readmemfl);
	if (arm.abort_base_restored) {
		gen_test_armirq();
	}
	// .nextbit
	gen_x86_jump_here(jump_nextbit);
	// Rotate if load is unaligned
	addbyte(0x89); addbyte(0xd9); // MOV %ebx,%ecx
	addbyte(0xc1); addbyte(0xe1); addbyte(3); // SHL $3,%ecx
	addbyte(0xd3); addbyte(0xc8); // ROR %cl,%eax
}

/**
 * Register usage:
 *	%ebx	addr
 *	%eax	data (out)
 */
static void
genldrb(void)
{
	int jump_nextbit, jump_notinbuffer;

	addbyte(0x89); addbyte(0xda); // MOV %ebx,%edx
	addbyte(0xc1); addbyte(0xea); addbyte(12); // SHR $12,%edx
	addbyte(0x8b); addbyte(0x14); addbyte(0x95); addptr(vraddrl); // MOV vraddrl(,%edx,4),%edx
	addbyte(0xf6); addbyte(0xc2); addbyte(1); // TEST $1,%dl
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x0f); addbyte(0xb6); addbyte(0x04); addbyte(0x1a); // MOVZB (%edx,%ebx),%eax
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	// .notinbuffer
	gen_x86_jump_here(jump_notinbuffer);
	gen_x86_mov_reg32_stack(EBX, 0);
	gen_x86_call(readmemfb);
	if (arm.abort_base_restored) {
		gen_test_armirq();
	}
	// .nextbit
	gen_x86_jump_here(jump_nextbit);
}

/**
 * Register usage:
 *	%ebx	addr
 *	%ecx	data
 */
static void
genstr(void)
{
	int jump_nextbit, jump_notinbuffer;

	addbyte(0x89); addbyte(0xda); // MOV %ebx,%edx
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xea); addbyte(12); // SHR $12,%edx
	addbyte(0x83); addbyte(0xe0); addbyte(0xfc); // AND $0xfffffffc,%eax
	addbyte(0x8b); addbyte(0x14); addbyte(0x95); addptr(vwaddrl); // MOV vwaddrl(,%edx,4),%edx
	addbyte(0xf6); addbyte(0xc2); addbyte(3); // TEST $3,%dl
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x89); addbyte(0x0c); addbyte(0x02); // MOV %ecx,(%edx,%eax)
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	// .notinbuffer
	gen_x86_jump_here(jump_notinbuffer);
	gen_x86_mov_reg32_stack(EAX, 0);
	gen_x86_mov_reg32_stack(ECX, 4);
	gen_x86_call(writememfl);
	if (arm.abort_base_restored) {
		gen_test_armirq();
	}
	// .nextbit
	gen_x86_jump_here(jump_nextbit);
}

/**
 * Register usage:
 *	%ebx	addr
 *	%ecx	data
 */
static void
genstrb(void)
{
	int jump_nextbit, jump_notinbuffer;

	addbyte(0x89); addbyte(0xda); // MOV %ebx,%edx
	addbyte(0xc1); addbyte(0xea); addbyte(12); // SHR $12,%edx
	addbyte(0x8b); addbyte(0x14); addbyte(0x95); addptr(vwaddrl); // MOV vwaddrl(,%edx,4),%edx
	addbyte(0xf6); addbyte(0xc2); addbyte(3); // TEST $3,%dl
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x88); addbyte(0x0c); addbyte(0x1a); // MOV %cl,(%edx,%ebx)
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	// .notinbuffer
	gen_x86_jump_here(jump_notinbuffer);
	gen_x86_mov_reg32_stack(EBX, 0);
	gen_x86_mov_reg32_stack(ECX, 4);
	gen_x86_call(writememfb);
	if (arm.abort_base_restored) {
		gen_test_armirq();
	}
	// .nextbit
	gen_x86_jump_here(jump_nextbit);
}

/**
 * Generate code to calculate the address and writeback values for a LDM/STM
 * decrement.
 *
 * Register usage:
 *	%ebx	addr (aligned)
 *	%edx	writeback
 *
 * @param opcode Opcode of instruction being emulated
 * @param offset Offset of transfer (transfer size)
 */
static void
gen_arm_ldm_stm_decrement(uint32_t opcode, uint32_t offset)
{
	gen_load_reg(RN, EBX);
	addbyte(0x83); addbyte(0xeb); addbyte(offset); // SUB $offset,%ebx
	addbyte(0x89); addbyte(0xda); // MOV %ebx,%edx
	if (!(opcode & (1u << 24))) {
		// Decrement After
		addbyte(0x83); addbyte(0xc3); addbyte(4); // ADD $4,%ebx
	}

	// Align addr
	addbyte(0x83); addbyte(0xe3); addbyte(0xfc); // AND $0xfffffffc,%ebx
}

/**
 * Generate code to calculate the address and writeback values for a LDM/STM
 * increment.
 *
 * Register usage:
 *	%ebx	addr (aligned)
 *	%edx	writeback
 *
 * @param opcode Opcode of instruction being emulated
 * @param offset Offset of transfer (transfer size)
 */
static void
gen_arm_ldm_stm_increment(uint32_t opcode, uint32_t offset)
{
	gen_load_reg(RN, EBX);
	addbyte(0x89); addbyte(0xda); // MOV %ebx,%edx
	addbyte(0x83); addbyte(0xc2); addbyte(offset); // ADD $offset,%edx
	if (opcode & (1u << 24)) {
		// Increment Before
		addbyte(0x83); addbyte(0xc3); addbyte(4); // ADD $4,%ebx
	}

	// Align addr
	addbyte(0x83); addbyte(0xe3); addbyte(0xfc); // AND $0xfffffffc,%ebx
}

/**
 * Generate code to call a LDM/STM helper function.
 *
 * Requires %ebx and %edx to contain values for 2nd and 3rd arguments.
 *
 * @param opcode    Opcode of instruction being emulated
 * @param helper_fn Pointer to function to call
 */
static void
gen_call_ldm_stm_helper(uint32_t opcode, const void *helper_fn)
{
	gen_x86_mov_reg32_stack(EDX, 8);
	gen_x86_mov_reg32_stack(EBX, 4);
	addbyte(0xc7); addbyte(0x04); addbyte(0x24); addlong(opcode); // MOVL $opcode,(%esp)

	gen_x86_call(helper_fn);

	gen_test_armirq();
}

/**
 * Generate code to perform a Store Multiple register operation when the S flag
 * is clear.
 *
 * Register usage:
 *	%ebx	addr
 *	%edx	writeback
 *	%eax	data (scratch)
 * Stack usage:
 *	0(%esp)	1st function call argument (opcode)
 *	4(%esp)	2nd function call argument (addr)
 *	8(%esp)	3rd function call argument (writeback)
 *
 * @param opcode Opcode of instruction being emulated
 * @param offset Offset of transfer (transfer size)
 */
static void
gen_arm_store_multiple(uint32_t opcode, uint32_t offset)
{
	int jump_page_boundary_cross, jump_tlb_miss, jump_done;
	uint32_t mask, d;
	int c;

	// Check if crossing Page boundary
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0x0d); addlong(0xfffffc00); // OR $0xfffffc00,%eax
	addbyte(0x83); addbyte(0xc0); addbyte(offset - 1); // ADD $(offset - 1),%eax
	jump_page_boundary_cross = gen_x86_jump_forward_long(CC_C);

	// TLB lookup
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xe8); addbyte(12); // SHR $12,%eax
	addbyte(0x8b); addbyte(0x04); addbyte(0x85); addptr(vwaddrl); // MOV vwaddrl(,%eax,4),%eax
	addbyte(0xa8); addbyte(0x03); // TEST $3,%al
	jump_tlb_miss = gen_x86_jump_forward_long(CC_NZ);

	// Convert TLB Page and Address to Host address
	addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx

	// Store first register
	mask = 1;
	d = 0;
	for (c = 0; c < 15; c++) {
		if (opcode & mask) {
			gen_load_reg(c, EAX);
			addbyte(0x89); addbyte(0x43); addbyte(d); // MOV %eax,d(%ebx)
			d += 4;
			c++;
			mask <<= 1;
			break;
		}
		mask <<= 1;
	}

	// Perform Writeback (if requested) at end of 2nd cycle
	if (!arm.stm_writeback_at_end && (opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	// Store remaining registers
	for ( ; c < 16; c++) {
		if (opcode & mask) {
			gen_load_reg(c, EAX);
			if ((c == 15) && (arm.r15_diff != 0)) {
				addbyte(0x83); addbyte(0xc0); addbyte(arm.r15_diff); // ADD $arm.r15_diff,%eax
			}
			addbyte(0x89); addbyte(0x43); addbyte(d); // MOV %eax,d(%ebx)
			d += 4;
		}
		mask <<= 1;
	}

	// Perform Writeback (if requested) at end of instruction (SA110)
	if (arm.stm_writeback_at_end && (opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	jump_done = gen_x86_jump_forward(CC_ALWAYS);

	// Call helper function
	gen_x86_jump_here_long(jump_page_boundary_cross);
	gen_x86_jump_here_long(jump_tlb_miss);
	gen_call_ldm_stm_helper(opcode, arm_store_multiple);

	// All done, continue here
	gen_x86_jump_here(jump_done);
}

/**
 * Generate code to perform a Store Multiple register operation when the S flag
 * is set.
 *
 * Register usage:
 *	%ebx	addr
 *	%edx	writeback
 *	%eax	data (scratch)
 *	%ecx	usrregs ptr
 * Stack usage:
 *	0(%esp)	1st function call argument (opcode)
 *	4(%esp)	2nd function call argument (addr)
 *	8(%esp)	3rd function call argument (writeback)
 *
 * @param opcode Opcode of instruction being emulated
 * @param offset Offset of transfer (transfer size)
 */
static void
gen_arm_store_multiple_s(uint32_t opcode, uint32_t offset)
{
	int jump_page_boundary_cross, jump_tlb_miss, jump_done;
	uint32_t mask, d;
	int c;

	// Check if crossing Page boundary
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0x0d); addlong(0xfffffc00); // OR $0xfffffc00,%eax
	addbyte(0x83); addbyte(0xc0); addbyte(offset - 1); // ADD $(offset - 1),%eax
	jump_page_boundary_cross = gen_x86_jump_forward_long(CC_C);

	// TLB lookup
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xe8); addbyte(12); // SHR $12,%eax
	addbyte(0x8b); addbyte(0x04); addbyte(0x85); addptr(vwaddrl); // MOV vwaddrl(,%eax,4),%eax
	addbyte(0xa8); addbyte(0x03); // TEST $3,%al
	jump_tlb_miss = gen_x86_jump_forward_long(CC_NZ);

	// Convert TLB Page and Address to Host address
	addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx

	// Store first register
	mask = 1;
	d = 0;
	for (c = 0; c < 15; c++) {
		if (opcode & mask) {
			addbyte(0x8b); addbyte(0x0d); addptr(&usrregs[c]); // MOV usrregs[c],%ecx
			addbyte(0x8b); addbyte(0x01); // MOV (%ecx),%eax
			addbyte(0x89); addbyte(0x43); addbyte(d); // MOV %eax,d(%ebx)
			d += 4;
			c++;
			mask <<= 1;
			break;
		}
		mask <<= 1;
	}

	// Perform Writeback (if requested) at end of 2nd cycle
	if (!arm.stm_writeback_at_end && (opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	// Store remaining registers
	for ( ; c < 16; c++) {
		if (opcode & mask) {
			addbyte(0x8b); addbyte(0x0d); addptr(&usrregs[c]); // MOV usrregs[c],%ecx
			addbyte(0x8b); addbyte(0x01); // MOV (%ecx),%eax
			if ((c == 15) && (arm.r15_diff != 0)) {
				addbyte(0x83); addbyte(0xc0); addbyte(arm.r15_diff); // ADD $arm.r15_diff,%eax
			}
			addbyte(0x89); addbyte(0x43); addbyte(d); // MOV %eax,d(%ebx)
			d += 4;
		}
		mask <<= 1;
	}

	// Perform Writeback (if requested) at end of instruction (SA110)
	if (arm.stm_writeback_at_end && (opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	jump_done = gen_x86_jump_forward(CC_ALWAYS);

	// Call helper function
	gen_x86_jump_here_long(jump_page_boundary_cross);
	gen_x86_jump_here_long(jump_tlb_miss);
	gen_call_ldm_stm_helper(opcode, arm_store_multiple_s);

	// All done, continue here
	gen_x86_jump_here(jump_done);
}

/**
 * Generate code to perform a Load Multiple register operation when the S flag
 * is clear.
 *
 * Register usage:
 *	%ebx	addr
 *	%edx	writeback
 *	%eax	data (scratch)
 *	%ecx	TLB page
 * Stack usage:
 *	0(%esp)	1st function call argument (opcode)
 *	4(%esp)	2nd function call argument (addr)
 *	8(%esp)	3rd function call argument (writeback)
 *
 * @param opcode Opcode of instruction being emulated
 * @param offset Offset of transfer (transfer size)
 */
static void
gen_arm_load_multiple(uint32_t opcode, uint32_t offset)
{
	int jump_page_boundary_cross, jump_tlb_miss, jump_done;
	uint32_t mask, d;
	int c;

	// Check if crossing Page boundary
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0x0d); addlong(0xfffffc00); // OR $0xfffffc00,%eax
	addbyte(0x83); addbyte(0xc0); addbyte(offset - 1); // ADD $(offset - 1),%eax
	jump_page_boundary_cross = gen_x86_jump_forward_long(CC_C);

	// TLB lookup
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xe8); addbyte(12); // SHR $12,%eax
	addbyte(0x8b); addbyte(0x04); addbyte(0x85); addptr(vraddrl); // MOV vraddrl(,%eax,4),%eax
	addbyte(0xa8); addbyte(0x01); // TEST $1,%al
	jump_tlb_miss = gen_x86_jump_forward_long(CC_NZ);

	// Convert TLB Page and Address to Host address
	addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx

	// Perform Writeback (if requested)
	if ((opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	// Load registers
	mask = 1;
	d = 0;
	for (c = 0; c < 16; c++) {
		if (opcode & mask) {
			addbyte(0x8b); addbyte(0x43); addbyte(d); // MOV d(%ebx),%eax
			if (c == 15) {
				addbyte(0x8b); addbyte(0x4e); addbyte(offsetof(ARMState, r15_mask)); // MOV arm.r15_mask,%ecx
				gen_load_reg(15, EDX);
				addbyte(0x83); addbyte(0xc0); addbyte(4); // ADD $4,%eax
				addbyte(0x21); addbyte(0xc8); // AND %ecx,%eax
				addbyte(0xf7); addbyte(0xd1); // NOT %ecx
				addbyte(0x21); addbyte(0xca); // AND %ecx,%edx
				addbyte(0x09); addbyte(0xd0); // OR %edx,%eax
			}
			gen_save_reg(c, EAX);
			d += 4;
		}
		mask <<= 1;
	}

	jump_done = gen_x86_jump_forward(CC_ALWAYS);

	// Call helper function
	gen_x86_jump_here_long(jump_page_boundary_cross);
	gen_x86_jump_here_long(jump_tlb_miss);
	gen_call_ldm_stm_helper(opcode, arm_load_multiple);

	// All done, continue here
	gen_x86_jump_here(jump_done);
}

/**
 * Generate code to perform a Load Multiple register operation when the S flag
 * is set.
 *
 * Register usage:
 *	%ebx	addr
 *	%edx	writeback
 *	%eax	data (scratch)
 *	%ecx	usrregs ptr
 * Stack usage:
 *	0(%esp)	1st function call argument (opcode)
 *	4(%esp)	2nd function call argument (addr)
 *	8(%esp)	3rd function call argument (writeback)
 *
 * @param opcode Opcode of instruction being emulated
 * @param offset Offset of transfer (transfer size)
 */
static void
gen_arm_load_multiple_s(uint32_t opcode, uint32_t offset)
{
	int jump_page_boundary_cross, jump_tlb_miss, jump_done;
	uint32_t mask, d;
	int c;

	// Check if crossing Page boundary
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0x0d); addlong(0xfffffc00); // OR $0xfffffc00,%eax
	addbyte(0x83); addbyte(0xc0); addbyte(offset - 1); // ADD $(offset - 1),%eax
	jump_page_boundary_cross = gen_x86_jump_forward_long(CC_C);

	// TLB lookup
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xe8); addbyte(12); // SHR $12,%eax
	addbyte(0x8b); addbyte(0x04); addbyte(0x85); addptr(vraddrl); // MOV vraddrl(,%eax,4),%eax
	addbyte(0xa8); addbyte(0x01); // TEST $1,%al
	jump_tlb_miss = gen_x86_jump_forward_long(CC_NZ);

	// Convert TLB Page and Address to Host address
	addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx

	// Perform Writeback (if requested)
	if ((opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	// Perform Load into User Bank
	mask = 1;
	d = 0;
	for (c = 0; c < 15; c++) {
		if (opcode & mask) {
			addbyte(0x8b); addbyte(0x0d); addptr(&usrregs[c]); // MOV usrregs[c],%ecx
			addbyte(0x8b); addbyte(0x43); addbyte(d); // MOV d(%ebx),%eax
			addbyte(0x89); addbyte(0x01); // MOV %eax,(%ecx)
			d += 4;
		}
		mask <<= 1;
	}

	jump_done = gen_x86_jump_forward(CC_ALWAYS);

	// Call helper function
	gen_x86_jump_here_long(jump_page_boundary_cross);
	gen_x86_jump_here_long(jump_tlb_miss);
	gen_call_ldm_stm_helper(opcode, arm_load_multiple_s);

	// All done, continue here
	gen_x86_jump_here(jump_done);
}

static int
recompile(uint32_t opcode, uint32_t *pcpsr)
{
	uint32_t rhs;
	uint32_t offset;

	if (arm.arch_v4) {
		if ((opcode & 0xe0000f0) == 0xb0) {
			// LDRH/STRH
			return 0;
		} else if ((opcode & 0xe1000d0) == 0x1000d0) {
			// LDRSB/LDRSH
			return 0;
		}
	}

	switch ((opcode >> 20) & 0xff) {
	case 0x00: // AND reg
		if ((opcode & 0xf0) == 0x90) {
			// MUL
			if (MULRD == MULRM) {
				return 0;
			}
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
			gen_save_reg(MULRD, EAX);
			break;
		}
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_AND, 0);
		break;

	case 0x01: // ANDS reg
		if ((opcode & 0xf0) == 0x90) {
			// MULS
			if (MULRD == MULRM) {
				return 0;
			}
			addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
			addbyte(0x81); addbyte(0xe1); addlong(0x3fffffff); // AND $~(NFLAG|ZFLAG),%ecx
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
			gen_save_reg(MULRD, EAX);
			addbyte(0x85); addbyte(0xc0); // TEST %eax,%eax
			gen_flags_logical(pcpsr);
			break;
		}
		if (RD == 15 || RN == 15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_AND, 0);
		gen_flags_logical(pcpsr);
		break;

	case 0x02: // EOR reg
		if ((opcode & 0xf0) == 0x90) {
			// MLA
			if (MULRD == MULRM) {
				return 0;
			}
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
			addbyte(0x03); addbyte(0x46); addbyte(MULRN<<2); // ADD Rn,%eax
			gen_save_reg(MULRD, EAX);
			break;
		}
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_XOR, 0);
		break;

	case 0x03: // EORS reg
		if ((opcode & 0xf0) == 0x90) {
			// MLAS
			if (MULRD == MULRM) {
				return 0;
			}
			addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
			addbyte(0x81); addbyte(0xe1); addlong(0x3fffffff); // AND $~(NFLAG|ZFLAG),%ecx
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
			addbyte(0x03); addbyte(0x46); addbyte(MULRN<<2); // ADD Rn,%eax
			gen_save_reg(MULRD, EAX);
			addbyte(0x85); addbyte(0xc0); // TEST %eax,%eax
			gen_flags_logical(pcpsr);
			break;
		}
		if (RD == 15 || RN == 15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_XOR, 0);
		gen_flags_logical(pcpsr);
		break;

	case 0x04: // SUB reg
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_SUB, 1);
		break;

	case 0x05: // SUBS reg
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		addbyte(0x81); addbyte(0xe1); addlong(0x0fffffff); // AND $0x0fffffff,%ecx
		gen_data_proc_reg(opcode, X86_OP_SUB, 1);
		gen_flags_sub(pcpsr);
		break;

	case 0x06: // RSB reg
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0x2b); addbyte(0x46); addbyte(RN<<2); // SUB Rn,%eax
		gen_save_reg(RD, EAX);
		break;

	case 0x08: // ADD reg
		if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
			// UMULL
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
			gen_save_reg(MULRN, EAX);
			gen_save_reg(MULRD, EDX);
			break;
		}
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_ADD, 0);
		break;

	case 0x09: // ADDS reg
		if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
			// UMULLS
			addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
			addbyte(0x81); addbyte(0xe1); addlong(0x3fffffff); // AND $~(NFLAG|ZFLAG),%ecx
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
			gen_save_reg(MULRN, EAX);
			gen_save_reg(MULRD, EDX);
			gen_flags_long_multiply(pcpsr);
			break;
		}
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		addbyte(0x81); addbyte(0xe1); addlong(0x0fffffff); // AND $0x0fffffff,%ecx
		gen_data_proc_reg(opcode, X86_OP_ADD, 0);
		gen_flags_add(pcpsr);
		break;

	case 0x0a: // ADC reg
		if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
			// UMLAL
			return 0;
		}
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		gen_load_reg(RN, EDX);
		addbyte(0xc1); addbyte(0xe1); addbyte(3); // SHL $3,%ecx - put ARM carry into x86 carry
		addbyte(0x11); addbyte(0xc2); // ADC %eax,%edx
		gen_save_reg(RD, EDX);
		break;

	case 0x0b: // ADCS reg
		if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
			// UMLALS
			return 0;
		}
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_load_reg(RN, EDX);
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		addbyte(0x89); addbyte(0xcb); // MOV %ecx,%ebx
		addbyte(0x81); addbyte(0xe1); addlong(0x0fffffff); // AND $~(NFLAG|ZFLAG|CFLAG|VFLAG),%ecx
		addbyte(0xc1); addbyte(0xe3); addbyte(3); // SHL $3,%ebx - put ARM carry into x86 carry
		addbyte(0x11); addbyte(0xc2); // ADC %eax,%edx
		gen_save_reg(RD, EDX);
		gen_flags_add(pcpsr);
		break;

	case 0x0c: // SBC reg
		if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
			// SMULL
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x6e); addbyte(MULRS<<2); // IMULL Rs
			gen_save_reg(MULRN, EAX);
			gen_save_reg(MULRD, EDX);
			break;
		}
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		gen_load_reg(RN, EDX);
		addbyte(0xc1); addbyte(0xe1); addbyte(3); // SHL $3,%ecx - put ARM carry into x86 carry
		gen_x86_cmc();
		addbyte(0x19); addbyte(0xc2); // SBB %eax,%edx
		gen_save_reg(RD, EDX);
		break;

	case 0x0e: // RSC reg
		if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
			// SMLAL
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x6e); addbyte(MULRS<<2); // IMULL Rs
			addbyte(0x01); addbyte(0x46); addbyte(MULRN<<2); // ADD %eax,Rn
			addbyte(0x11); addbyte(0x56); addbyte(MULRD<<2); // ADC %edx,Rd
			break;
		}
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		gen_load_reg(RN, EDX);
		addbyte(0xc1); addbyte(0xe1); addbyte(3); // SHL $3,%ecx - put ARM carry into x86 carry
		gen_x86_cmc();
		addbyte(0x19); addbyte(0xd0); // SBB %edx,%eax
		gen_save_reg(RD, EAX);
		break;

	case 0x11: // TST reg
		if (RD == 15 || RN == 15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) {
			return 0;
		}
		addbyte(0x85); addbyte(0x46); addbyte(RN<<2); // TEST %eax,Rn
		gen_flags_logical(pcpsr);
		break;

	case 0x13: // TEQ reg
		if (RD == 15 || RN == 15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) {
			return 0;
		}
		addbyte(0x33); addbyte(0x46); addbyte(RN<<2); // XOR Rn,%eax
		gen_flags_logical(pcpsr);
		break;

	case 0x15: // CMP reg
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		addbyte(0x81); addbyte(0xe1); addlong(0x0fffffff); // AND $0x0fffffff,%ecx
		gen_load_reg(RN, EDX);
		addbyte(0x29); addbyte(0xc2); // SUB %eax,%edx
		gen_flags_sub(pcpsr);
		break;

	case 0x18: // ORR reg
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_OR, 0);
		break;

	case 0x19: // ORRS reg
		if (RD == 15 || RN == 15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_OR, 0);
		gen_flags_logical(pcpsr);
		break;

	case 0x1a: // MOV reg
		if (!generate_shift(opcode)) {
			return 0;
		}
		if (RD == 15) {
			gen_load_reg(15, EDX);
			addbyte(0x83); addbyte(0xc0); addbyte(4); // ADD $4,%eax
			addbyte(0x81); addbyte(0xe2); addlong(~arm.r15_mask); // AND $(~arm.r15_mask),%edx
			addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
			addbyte(0x09); addbyte(0xd0); // OR %edx,%eax
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x1b: // MOVS reg
		if (RD == 15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) {
			return 0;
		}
		addbyte(0x85); addbyte(0xc0); // TEST %eax,%eax
		gen_save_reg(RD, EAX);
		gen_flags_logical(pcpsr);
		break;

	case 0x1c: // BIC reg
		if (RD == 15 || RN == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0xf7); addbyte(0xd0); // NOT %eax
		gen_data_proc_reg(opcode, X86_OP_AND, 0);
		break;

	case 0x1d: // BICS reg
		if (RD == 15 || RN == 15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) {
			return 0;
		}
		addbyte(0xf7); addbyte(0xd0); // NOT %eax
		gen_data_proc_reg(opcode, X86_OP_AND, 0);
		gen_flags_logical(pcpsr);
		break;

	case 0x1e: // MVN reg
		if (RD == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0xf7); addbyte(0xd0); // NOT %eax
		gen_save_reg(RD, EAX);
		break;

	case 0x1f: // MVNS reg
		if (RD == 15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) {
			return 0;
		}
		addbyte(0xf7); addbyte(0xd0); // NOT %eax
		addbyte(0x85); addbyte(0xc0); // TEST %eax,%eax
		gen_save_reg(RD, EAX);
		gen_flags_logical(pcpsr);
		break;

	case 0x20: // AND imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_AND, rhs);
		break;

	case 0x21: // ANDS imm
		if (RD == 15) return 0;
		rhs = gen_imm_cflag(opcode, pcpsr);
		gen_data_proc_imm(opcode, X86_OP_AND, rhs);
		gen_flags_logical(pcpsr);
		break;

	case 0x22: // EOR imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_XOR, rhs);
		break;

	case 0x23: // EORS imm
		if (RD == 15) return 0;
		rhs = gen_imm_cflag(opcode, pcpsr);
		gen_data_proc_imm(opcode, X86_OP_XOR, rhs);
		gen_flags_logical(pcpsr);
		break;

	case 0x24: // SUB imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_SUB, rhs);
		break;

	case 0x25: // SUBS imm
		if (RD == 15) return 0;
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		addbyte(0x81); addbyte(0xe1); addlong(0x0fffffff); // AND $0x0fffffff,%ecx
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_SUB, rhs);
		gen_flags_sub(pcpsr);
		break;

	case 0x28: // ADD imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_ADD, rhs);
		break;

	case 0x29: // ADDS imm
		if (RD == 15) return 0;
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		addbyte(0x81); addbyte(0xe1); addlong(0x0fffffff); // AND $0x0fffffff,%ecx
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_ADD, rhs);
		gen_flags_add(pcpsr);
		break;

	case 0x31: // TST imm
		if (RD == 15) return 0;
		rhs = gen_imm_cflag(opcode, pcpsr);
		gen_load_reg(RN, EAX);
		if (RN == 15 && arm.r15_mask != 0xfffffffc) {
			addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
		}
		addbyte(0xa9); addlong(rhs); // TEST $rhs,%eax
		gen_flags_logical(pcpsr);
		break;

	case 0x33: // TEQ imm
		if (RD == 15) return 0;
		rhs = gen_imm_cflag(opcode, pcpsr);
		gen_load_reg(RN, EAX);
		if (RN == 15 && arm.r15_mask != 0xfffffffc) {
			addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
		}
		addbyte(0x35); addlong(rhs); // XOR $rhs,%eax
		gen_flags_logical(pcpsr);
		break;

	case 0x35: // CMP imm
		if (RD == 15 || RN == 15) return 0;
		addbyte(0x8b); addbyte(0x0d); addptr(pcpsr); // MOV pcpsr,%ecx
		addbyte(0x81); addbyte(0xe1); addlong(0x0fffffff); // AND $0x0fffffff,%ecx
		rhs = arm_imm(opcode);
		gen_load_reg(RN, EAX);
		addbyte(0x3d); addlong(rhs); // CMP $rhs,%eax
		gen_flags_sub(pcpsr);
		break;

	case 0x38: // ORR imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_OR, rhs);
		break;

	case 0x39: // ORRS imm
		if (RD == 15) return 0;
		rhs = gen_imm_cflag(opcode, pcpsr);
		gen_data_proc_imm(opcode, X86_OP_OR, rhs);
		gen_flags_logical(pcpsr);
		break;

	case 0x3a: // MOV imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		addbyte(0xc7); addbyte(0x46); addbyte(RD<<2); addlong(rhs); // MOVL $rhs,Rd
		break;

	case 0x3b: // MOVS imm
		if (RD == 15) return 0;
		rhs = gen_imm_cflag(opcode, pcpsr);
		if (rhs == 0) {
			addbyte(0x81); addbyte(0xc9); addlong(ZFLAG); // OR $ZFLAG,%ecx
		} else if (rhs & 0x80000000) {
			addbyte(0x81); addbyte(0xc9); addlong(NFLAG); // OR $NFLAG,%ecx
		}
		addbyte(0xc7); addbyte(0x46); addbyte(RD<<2); addlong(rhs); // MOVL $rhs,Rd
		addbyte(0x89); addbyte(0x0d); addptr(pcpsr); // MOV %ecx,pcpsr
		break;

	case 0x3c: // BIC imm
		if (RD == 15) return 0;
		rhs = ~arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_AND, rhs);
		break;

	case 0x3d: // BICS imm
		if (RD == 15) return 0;
		rhs = ~gen_imm_cflag(opcode, pcpsr);
		gen_data_proc_imm(opcode, X86_OP_AND, rhs);
		gen_flags_logical(pcpsr);
		break;

	case 0x3e: // MVN imm
		if (RD == 15) return 0;
		rhs = ~arm_imm(opcode);
		addbyte(0xc7); addbyte(0x46); addbyte(RD<<2); addlong(rhs); // MOVL $rhs,Rd
		break;

	case 0x3f: // MVNS imm
		if (RD == 15) return 0;
		rhs = ~gen_imm_cflag(opcode, pcpsr);
		// Not possible for 'rhs' to be zero here, so no need to set Z flag
		if (rhs & 0x80000000) {
			addbyte(0x81); addbyte(0xc9); addlong(NFLAG); // OR $NFLAG,%ecx
		}
		addbyte(0xc7); addbyte(0x46); addbyte(RD<<2); addlong(rhs); // MOVL $rhs,Rd
		addbyte(0x89); addbyte(0x0d); addptr(pcpsr); // MOV %ecx,pcpsr
		break;

	case 0x40: // STR Rd, [Rn], #-imm
	case 0x48: // STR Rd, [Rn], #+imm
	case 0x60: // STR Rd, [Rn], -reg...
	case 0x68: // STR Rd, [Rn], +reg...
		if (RD == 15 || RN == 15) {
			return 0;
		}
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode)) {
				return 0;
			}
			gen_x86_mov_reg32_stack(EAX, 8);
		}
		gen_load_reg(RN, EBX);
		gen_load_reg(RD, ECX);
		genstr();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EAX, 8);
			if (opcode & 0x800000) {
				addbyte(0x01); addbyte(0x46); addbyte(RN<<2); // ADD %eax,Rn
			} else {
				addbyte(0x29); addbyte(0x46); addbyte(RN<<2); // SUB %eax,Rn
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x46); // ADD
				} else {
					addbyte(0x6e); // SUB
				}
				addbyte(RN<<2); addlong(offset);
			}
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		break;

	case 0x44: // STRB Rd, [Rn], #-imm
	case 0x4c: // STRB Rd, [Rn], #+imm
	case 0x64: // STRB Rd, [Rn], -reg...
	case 0x6c: // STRB Rd, [Rn], +reg...
		if (RD == 15 || RN == 15) {
			return 0;
		}
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode)) {
				return 0;
			}
			gen_x86_mov_reg32_stack(EAX, 8);
		}
		gen_load_reg(RN, EBX);
		gen_load_reg(RD, ECX);
		genstrb();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EAX, 8);
			if (opcode & 0x800000) {
				addbyte(0x01); addbyte(0x46); addbyte(RN<<2); // ADD %eax,Rn
			} else {
				addbyte(0x29); addbyte(0x46); addbyte(RN<<2); // SUB %eax,Rn
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x46); // ADD
				} else {
					addbyte(0x6e); // SUB
				}
				addbyte(RN<<2); addlong(offset);
			}
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		break;

	case 0x41: // LDR Rd, [Rn], #-imm
	case 0x49: // LDR Rd, [Rn], #+imm
	case 0x61: // LDR Rd, [Rn], -reg...
	case 0x69: // LDR Rd, [Rn], +reg...
		if (RD == 15 || RN == 15) {
			return 0;
		}
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode)) {
				return 0;
			}
			gen_x86_mov_reg32_stack(EAX, 8);
		}
		gen_load_reg(RN, EBX);
		genldr();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EDX, 8);
			if (opcode & 0x800000) {
				addbyte(0x01); addbyte(0x56); addbyte(RN<<2); // ADD %edx,Rn
			} else {
				addbyte(0x29); addbyte(0x56); addbyte(RN<<2); // SUB %edx,Rn
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x46); // ADD
				} else {
					addbyte(0x6e); // SUB
				}
				addbyte(RN<<2); addlong(offset);
			}
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x45: // LDRB Rd, [Rn], #-imm
	case 0x4d: // LDRB Rd, [Rn], #+imm
	case 0x65: // LDRB Rd, [Rn], -reg...
	case 0x6d: // LDRB Rd, [Rn], +reg...
		if (RD == 15 || RN == 15) {
			return 0;
		}
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode)) {
				return 0;
			}
			gen_x86_mov_reg32_stack(EAX, 8);
		}
		gen_load_reg(RN, EBX);
		genldrb();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EDX, 8);
			if (opcode & 0x800000) {
				addbyte(0x01); addbyte(0x56); addbyte(RN<<2); // ADD %edx,Rn
			} else {
				addbyte(0x29); addbyte(0x56); addbyte(RN<<2); // SUB %edx,Rn
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x46); // ADD
				} else {
					addbyte(0x6e); // SUB
				}
				addbyte(RN<<2); addlong(offset);
			}
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x50: // STR Rd, [Rn, #-imm]
	case 0x52: // STR Rd, [Rn, #-imm]!
	case 0x58: // STR Rd, [Rn, #+imm]
	case 0x5a: // STR Rd, [Rn, #+imm]!
	case 0x70: // STR Rd, [Rn, -reg...]
	case 0x72: // STR Rd, [Rn, -reg...]!
	case 0x78: // STR Rd, [Rn, +reg...]
	case 0x7a: // STR Rd, [Rn, +reg...]!
		if (RD == 15) {
			return 0;
		}
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode)) {
				return 0;
			}
		} else {
			addbyte(0xb8); addlong(opcode & 0xfff); // MOV $(opcode & 0xfff),%eax
		}
		gen_load_reg(RN, EBX);
		if (RN == 15) {
			addbyte(0x81); addbyte(0xe3); addlong(arm.r15_mask); // AND $arm.r15_mask,%ebx
		}
		if (opcode & 0x800000) {
			addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx
		} else {
			addbyte(0x29); addbyte(0xc3); // SUB %eax,%ebx
		}
		gen_load_reg(RD, ECX);
		genstr();
		if (opcode & 0x200000) {
			// Writeback
			gen_save_reg(RN, EBX);
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		break;

	case 0x54: // STRB Rd, [Rn, #-imm]
	case 0x56: // STRB Rd, [Rn, #-imm]!
	case 0x5c: // STRB Rd, [Rn, #+imm]
	case 0x5e: // STRB Rd, [Rn, #+imm]!
	case 0x74: // STRB Rd, [Rn, -reg...]
	case 0x76: // STRB Rd, [Rn, -reg...]!
	case 0x7c: // STRB Rd, [Rn, +reg...]
	case 0x7e: // STRB Rd, [Rn, +reg...]!
		if (RD == 15) {
			return 0;
		}
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode)) {
				return 0;
			}
		} else {
			addbyte(0xb8); addlong(opcode & 0xfff); // MOV $(opcode & 0xfff),%eax
		}
		gen_load_reg(RN, EBX);
		if (RN == 15) {
			addbyte(0x81); addbyte(0xe3); addlong(arm.r15_mask); // AND $arm.r15_mask,%ebx
		}
		if (opcode & 0x800000) {
			addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx
		} else {
			addbyte(0x29); addbyte(0xc3); // SUB %eax,%ebx
		}
		gen_load_reg(RD, ECX);
		genstrb();
		if (opcode & 0x200000) {
			// Writeback
			gen_save_reg(RN, EBX);
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		break;

	case 0x51: // LDR Rd, [Rn, #-imm]
	case 0x53: // LDR Rd, [Rn, #-imm]!
	case 0x59: // LDR Rd, [Rn, #+imm]
	case 0x5b: // LDR Rd, [Rn, #+imm]!
	case 0x71: // LDR Rd, [Rn, -reg...]
	case 0x73: // LDR Rd, [Rn, -reg...]!
	case 0x79: // LDR Rd, [Rn, +reg...]
	case 0x7b: // LDR Rd, [Rn, +reg...]!
		if (RD == 15) {
			return 0;
		}
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode)) {
				return 0;
			}
		} else {
			addbyte(0xb8); addlong(opcode & 0xfff); // MOV $(opcode & 0xfff),%eax
		}
		gen_load_reg(RN, EBX);
		if (RN == 15) {
			addbyte(0x81); addbyte(0xe3); addlong(arm.r15_mask); // AND $arm.r15_mask,%ebx
		}
		if (opcode & 0x800000) {
			addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx
		} else {
			addbyte(0x29); addbyte(0xc3); // SUB %eax,%ebx
		}
		genldr();
		if (opcode & 0x200000) {
			// Writeback
			gen_save_reg(RN, EBX);
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x55: // LDRB Rd, [Rn, #-imm]
	case 0x57: // LDRB Rd, [Rn, #-imm]!
	case 0x5d: // LDRB Rd, [Rn, #+imm]
	case 0x5f: // LDRB Rd, [Rn, #+imm]!
	case 0x75: // LDRB Rd, [Rn, -reg...]
	case 0x77: // LDRB Rd, [Rn, -reg...]!
	case 0x7d: // LDRB Rd, [Rn, +reg...]
	case 0x7f: // LDRB Rd, [Rn, +reg...]!
		if (RD == 15) {
			return 0;
		}
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode)) {
				return 0;
			}
		} else {
			addbyte(0xb8); addlong(opcode & 0xfff); // MOV $(opcode & 0xfff),%eax
		}
		gen_load_reg(RN, EBX);
		if (RN == 15) {
			addbyte(0x81); addbyte(0xe3); addlong(arm.r15_mask); // AND $arm.r15_mask,%ebx
		}
		if (opcode & 0x800000) {
			addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx
		} else {
			addbyte(0x29); addbyte(0xc3); // SUB %eax,%ebx
		}
		genldrb();
		if (opcode & 0x200000) {
			// Writeback
			gen_save_reg(RN, EBX);
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x80: // STMDA
	case 0x82: // STMDA !
	case 0x90: // STMDB
	case 0x92: // STMDB !
		if (RN == 15) {
			return 0;
		}
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_decrement(opcode, offset);
		gen_arm_store_multiple(opcode, offset);
		break;

	case 0x84: // STMDA ^
	case 0x86: // STMDA ^!
	case 0x94: // STMDB ^
	case 0x96: // STMDB ^!
		if (RN == 15) {
			return 0;
		}
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_decrement(opcode, offset);
		gen_arm_store_multiple_s(opcode, offset);
		break;

	case 0x88: // STMIA
	case 0x8a: // STMIA !
	case 0x98: // STMIB
	case 0x9a: // STMIB !
		if (RN == 15) {
			return 0;
		}
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_increment(opcode, offset);
		gen_arm_store_multiple(opcode, offset);
		break;

	case 0x8c: // STMIA ^
	case 0x8e: // STMIA ^!
	case 0x9c: // STMIB ^
	case 0x9e: // STMIB ^!
		if (RN == 15) {
			return 0;
		}
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_increment(opcode, offset);
		gen_arm_store_multiple_s(opcode, offset);
		break;

	case 0x81: // LDMDA
	case 0x83: // LDMDA !
	case 0x91: // LDMDB
	case 0x93: // LDMDB !
		if (RN == 15) {
			return 0;
		}
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_decrement(opcode, offset);
		gen_arm_load_multiple(opcode, offset);
		break;

	case 0x85: // LDMDA ^
	case 0x87: // LDMDA ^!
	case 0x95: // LDMDB ^
	case 0x97: // LDMDB ^!
		if (RN == 15 || (opcode & 0x8000)) {
			return 0;
		}
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_decrement(opcode, offset);
		gen_arm_load_multiple_s(opcode, offset);
		break;

	case 0x89: // LDMIA
	case 0x8b: // LDMIA !
	case 0x99: // LDMIB
	case 0x9b: // LDMIB !
		if (RN == 15) {
			return 0;
		}
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_increment(opcode, offset);
		gen_arm_load_multiple(opcode, offset);
		break;

	case 0x8d: // LDMIA ^
	case 0x8f: // LDMIA ^!
	case 0x9d: // LDMIB ^
	case 0x9f: // LDMIB ^!
		if (RN == 15 || (opcode & 0x8000)) {
			return 0;
		}
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_increment(opcode, offset);
		gen_arm_load_multiple_s(opcode, offset);
		break;

	case 0xa0: case 0xa1: case 0xa2: case 0xa3: // B
	case 0xa4: case 0xa5: case 0xa6: case 0xa7:
	case 0xa8: case 0xa9: case 0xaa: case 0xab:
	case 0xac: case 0xad: case 0xae: case 0xaf:
		offset = (opcode << 8);
		offset = (uint32_t) ((int32_t) offset >> 6);
		offset += 4;
		if (!flaglookup[opcode >> 28][(*pcpsr) >> 28] && pcinc != 0) {
			offset += pcinc;
		}
		if (((PC + offset) & 0xfc000000) == 0) {
			if (offset < 0x80) {
				addbyte(0x83); addbyte(0x46); addbyte(15<<2); addbyte(offset); // ADDL $offset,R15
			} else {
				addbyte(0x81); addbyte(0x46); addbyte(15<<2); addlong(offset); // ADDL $offset,R15
			}
		} else {
			gen_load_reg(15, EAX);
			if (arm.r15_mask != 0xfffffffc) {
				addbyte(0x89); addbyte(0xc2); // MOV %eax,%edx
			}
			addbyte(0x05); addlong(offset); // ADD $offset,%eax
			if (arm.r15_mask != 0xfffffffc) {
				addbyte(0x81); addbyte(0xe2); addlong(0xfc000003); // AND $0xfc000003,%edx
				addbyte(0x25); addlong(0x03fffffc); // AND $0x03fffffc,%eax
				addbyte(0x09); addbyte(0xd0); // OR %edx,%eax
			}
			gen_save_reg(15, EAX);
		}
#if 0
		if ((PC + offset + 4) == currentblockpc2 && flaglookup[opcode >> 28][(*pcpsr) >> 28]) {
			// rpclog("Possible %07X %07X %08X\n",PC,currentblockpc,&rcodeblock[blockpoint2][codeblockpos]);
			addbyte(0xff); addbyte(0x0d); addptr(&linecyc); // DECL linecyc
			addbyte(0x78); addbyte(12); // JS endit
			addbyte(0x83); addbyte(0x05); addptr(&arm.reg[15]); addbyte(4); // ADDL $4,arm.reg[15]
			gen_x86_jump(CC_ALWAYS, block_enter); // JMP start
			// .endit
		}
#endif
		if (!flaglookup[opcode >> 28][(*pcpsr) >> 28]) {
			gen_x86_jump(CC_ALWAYS, 8);
		}
		break;

	case 0xb0: case 0xb1: case 0xb2: case 0xb3: // BL
	case 0xb4: case 0xb5: case 0xb6: case 0xb7:
	case 0xb8: case 0xb9: case 0xba: case 0xbb:
	case 0xbc: case 0xbd: case 0xbe: case 0xbf:
		offset = (opcode << 8);
		offset = (uint32_t) ((int32_t) offset >> 6);
		offset += 4;
		if (!flaglookup[opcode >> 28][(*pcpsr) >> 28] && pcinc != 0) {
			offset += pcinc;
		}
		gen_load_reg(15, EAX);
		addbyte(0x83); addbyte(0xe8); addbyte(4); // SUB $4,%eax
		if (((PC + offset) & 0xfc000000) == 0) {
			if (offset < 0x80) {
				addbyte(0x83); addbyte(0x46); addbyte(15<<2); addbyte(offset); // ADDL $offset,R15
			} else {
				addbyte(0x81); addbyte(0x46); addbyte(15<<2); addlong(offset); // ADDL $offset,R15
			}
			gen_save_reg(14, EAX);
		} else {
			gen_save_reg(14, EAX);
			gen_load_reg(15, EAX);
			if (arm.r15_mask != 0xfffffffc) {
				addbyte(0x89); addbyte(0xc2); // MOV %eax,%edx
			}
			addbyte(0x05); addlong(offset); // ADD $offset,%eax
			if (arm.r15_mask != 0xfffffffc) {
				addbyte(0x81); addbyte(0xe2); addlong(0xfc000003); // AND $0xfc000003,%edx
				addbyte(0x25); addlong(0x03fffffc); // AND $0x03fffffc,%eax
				addbyte(0x09); addbyte(0xd0); // OR %edx,%eax
			}
			gen_save_reg(15, EAX);
		}
		if (!flaglookup[opcode >> 28][(*pcpsr) >> 28]) {
			gen_x86_jump(CC_ALWAYS, 8);
		}
		break;

	default:
		return 0;
	}
	lastrecompiled = 1;
	if (lastflagchange != 0) {
		gen_x86_jump_here_long(lastflagchange);
	}
	return 1;
}

void
generatecall(OpFn addr, uint32_t opcode, uint32_t *pcpsr)
{
	const int old = codeblockpos;

	lastrecompiled = 0;

	if (recompileinstructions[(opcode >> 20) & 0xff]) {
		if (recompile(opcode, pcpsr)) {
			return;
		}
	}

	codeblockpos = old;

	addbyte(0xc7); addbyte(0x04); addbyte(0x24); addlong(opcode); // MOVL $opcode,(%esp)
	gen_x86_call(addr);

	if (!flaglookup[opcode >> 28][(*pcpsr) >> 28] && (opcode & 0xe000000) == 0xa000000) {
		if (pcinc != 0) {
			addbyte(0x83); addbyte(0x46); addbyte(15<<2); addbyte(pcinc); // ADDL $pcinc,R15
			// pcinc = 0;
		}
		gen_x86_jump(CC_ALWAYS, 8);
	}
	if (lastflagchange != 0) {
		gen_x86_jump_here_long(lastflagchange);
	}
}

void
generateupdatepc(void)
{
	if (pcinc != 0) {
		addbyte(0x83); addbyte(0x46); addbyte(15<<2); addbyte(pcinc); // ADDL $pcinc,R15
		pcinc = 0;
	}
}

void
generateupdateinscount(void)
{
	if (tempinscount != 0) {
		if (tempinscount > 127) {
			addbyte(0x81); addbyte(0x05); addptr(&inscount); addlong(tempinscount); // ADDL $tempinscount,inscount
		} else {
			addbyte(0x83); addbyte(0x05); addptr(&inscount); addbyte((uint8_t) tempinscount); // ADDL $tempinscount,inscount
		}
		tempinscount = 0;
	}
}

void
generatepcinc(void)
{
	tempinscount++;
	pcinc += 4;
	if (pcinc == 124) {
		generateupdatepc();
	}
	if (codeblockpos >= 1800) {
		blockend = 1;
	}
}

void
endblock(uint32_t opcode)
{
	generateupdatepc();
	generateupdateinscount();

	gen_x86_jump_here_long(9);

	addbyte(0xff); addbyte(0x0d); addptr(&linecyc); // DECL linecyc
	gen_x86_jump(CC_S, 0);

	addbyte(0xf7); addbyte(0x46); addbyte(offsetof(ARMState, event)); addlong(0xff); // TESTL $0xff,arm.event
	gen_x86_jump(CC_NZ, 0);

	gen_load_reg(15, EAX);
	if (arm.r15_mask != 0xfffffffc) {
		addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
	}

	if (((opcode >> 20) & 0xff) == 0xaf) {
		addbyte(0x3d); addlong(currentblockpc); // CMP $currentblockpc,%eax
		gen_x86_jump(CC_E, block_enter);
	}

	addbyte(0x83); addbyte(0xe8); addbyte(8); // SUB $8,%eax
	addbyte(0x89); addbyte(0xc2); // MOV %eax,%edx
	addbyte(0x81); addbyte(0xe2); addlong(0x1fffc); // AND $0x1fffc,%edx
	addbyte(0x3b); addbyte(0x82); addptr(codeblockpc); // CMP codeblockpc(%edx),%eax
	gen_x86_jump(CC_NE, 0);

	addbyte(0x8b); addbyte(0x82); addptr(codeblocknum); // MOV codeblocknum(%edx),%eax
	addbyte(0x8b); addbyte(0x04); addbyte(0x85); addptr(codeblockaddr); // MOV codeblockaddr(,%eax,4),%eax

	// Jump to next block bypassing function prologue
	addbyte(0x83); addbyte(0xc0); addbyte(block_enter); // ADD $block_enter,%eax
	addbyte(0xff); addbyte(0xe0); // JMP *%eax
}

void
generateflagtestandbranch(uint32_t opcode, uint32_t *pcpsr)
{
	int cond;

	if ((opcode >> 28) == 0xe) {
		// No need if 'always' condition code
		return;
	}
	switch (opcode >> 28) {
	case 0: // EQ
	case 1: // NE
		addbyte(0xf6); addbyte(0x05); addptr(((char *) pcpsr) + 3); addbyte(0x40); // TESTB $0x40,pcpsr+3
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	case 2: // CS
	case 3: // CC
		addbyte(0xf6); addbyte(0x05); addptr(((char *) pcpsr) + 3); addbyte(0x20); // TESTB $0x20,pcpsr+3
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	case 4: // MI
	case 5: // PL
		addbyte(0xf6); addbyte(0x05); addptr(((char *) pcpsr) + 3); addbyte(0x80); // TESTB $0x80,pcpsr+3
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	case 6: // VS
	case 7: // VC
		addbyte(0xf6); addbyte(0x05); addptr(((char *) pcpsr) + 3); addbyte(0x10); // TESTB $0x10,pcpsr+3
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	default:
		addbyte(0xa1); addptr(pcpsr); // MOV pcpsr,%eax
		addbyte(0xc1); addbyte(0xe8); addbyte(28); // SHR $28,%eax
		addbyte(0x80); addbyte(0xb8); addptr(&flaglookup[opcode >> 28][0]); addbyte(0); // CMPB $0,flaglookup(%eax)
		cond = CC_E;
		break;
	}
	lastflagchange = gen_x86_jump_forward_long(cond);
}

void
generateirqtest(void)
{
	if (lastrecompiled) {
		return;
	}

	addbyte(0x85); addbyte(0xc0); // TEST %eax,%eax
	gen_x86_jump(CC_NE, 0);
	if (lastflagchange != 0) {
		gen_x86_jump_here_long(lastflagchange);
	}
}
