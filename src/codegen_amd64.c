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

/*r15 is pointer to ARMState
  r14 is vwaddrl
  r13 is vraddrl
  r12 contains R15*/

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rpcemu.h"
#include "arm.h"
#include "arm_common.h"
#include "codegen_amd64.h"
#include "mem.h"

int lastflagchange;

uint8_t rcodeblock[BLOCKS][1792] __attribute__ ((aligned (4096)));
static const void *codeblockaddr[BLOCKS];
uint32_t codeblockpc[0x8000];
int codeblocknum[0x8000];
static uint8_t codeblockpresent[0x10000];

//#define BLOCKS 4096
//#define HASH(l) ((l>>3)&0x3fff)

static int blocknum;//,blockcount;
static int tempinscount;

static int codeblockpos;
static int lastjumppos;

static int blockpoint, blockpoint2;
static uint32_t blocks[BLOCKS];
static int pcinc;
static int lastrecompiled;
static int block_enter;

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
addptr64(const void *a)
{
	memcpy(&rcodeblock[blockpoint2][codeblockpos], &a, sizeof(uint64_t));
	codeblockpos += 8;
}

static inline void
addrip(const void *addr)
{
	const ptrdiff_t rel = ((const char *) addr) -
	                      ((const char *) &rcodeblock[blockpoint2][codeblockpos]);
	addlong((uint32_t) (rel - 4));
}

static inline void
addrip_byte(const void *addr, uint8_t x)
{
	const ptrdiff_t rel = ((const char *) addr) -
	                      ((const char *) &rcodeblock[blockpoint2][codeblockpos]);
	addlong((uint32_t) (rel - 5));
	addbyte(x);
}

static inline void
addrip_long(const void *addr, uint32_t x)
{
	const ptrdiff_t rel = ((const char *) addr) -
	                      ((const char *) &rcodeblock[blockpoint2][codeblockpos]);
	addlong((uint32_t) (rel - 8));
	addlong(x);
}

#include "codegen_x86_common.h"

// AMD64 registers and aliases
#define RAX	EAX
#define RCX	ECX
#define RDX	EDX
#define RBX	EBX
#define RSP	ESP
#define RBP	EBP
#define RSI	ESI
#define RDI	EDI
#define R8	8
#define R9	9
#define R10	10
#define R11	11
#define R12	12
#define R13	13
#define R14	14
#define R15	15

static inline void
gen_x86_push_reg(int x86reg)
{
	if (x86reg >= 8) {
		addbyte(0x41);
	}
	addbyte(0x50 | (x86reg & 0x7));
}

static inline void
gen_x86_pop_reg(int x86reg)
{
	if (x86reg >= 8) {
		addbyte(0x41);
	}
	addbyte(0x58 | (x86reg & 0x7));
}

static inline void
gen_x86_mov_reg32_stack(int x86reg, int offset)
{
	if (x86reg >= 8) {
		addbyte(0x44);
	}
	addbyte(0x89);
	if (offset != 0) {
		addbyte(0x44 | ((x86reg & 0x7) << 3)); addbyte(0x24); addbyte(offset);
	} else {
		addbyte(0x04 | ((x86reg & 0x7) << 3)); addbyte(0x24);
	}
}

static inline void
gen_x86_mov_stack_reg32(int x86reg, int offset)
{
	if (x86reg >= 8) {
		addbyte(0x44);
	}
	addbyte(0x8b);
	if (offset != 0) {
		addbyte(0x44 | ((x86reg & 0x7) << 3)); addbyte(0x24); addbyte(offset);
	} else {
		addbyte(0x04 | ((x86reg & 0x7) << 3)); addbyte(0x24);
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
	addbyte(0x45); addbyte(0x89); addbyte(0x67); addbyte(15<<2); // MOV %r12d,R15
	addbyte(0x48); addbyte(0x83); addbyte(0xc4); addbyte(8); // ADD $8,%rsp
	// Restore registers
	gen_x86_pop_reg(RBX);
	gen_x86_pop_reg(R12);
	gen_x86_pop_reg(R13);
	gen_x86_pop_reg(R14);
	gen_x86_pop_reg(R15);
	gen_x86_leave();
	gen_x86_ret();

	// Block Prologue
	assert(codeblockpos <= BLOCKSTART);
	codeblockpos = BLOCKSTART;
	// Set up a stack frame and preserve registers that are callee-saved
	gen_x86_push_reg(RBP);
	addbyte(0x48); addbyte(0x89); addbyte(0xe5); // MOV %rsp,%rbp
	gen_x86_push_reg(R15);
	gen_x86_push_reg(R14);
	gen_x86_push_reg(R13);
	gen_x86_push_reg(R12);
	gen_x86_push_reg(RBX);
	// Align stack to a multiple of 16 bytes - required by AMD64 ABI
	addbyte(0x48); addbyte(0x83); addbyte(0xec); addbyte(8); // SUB $8,%rsp

	addbyte(0x49); addbyte(0xbf); addptr64(&arm); // MOVABS $(&arm),%r15
	addbyte(0x49); addbyte(0xbe); addptr64(&vwaddrl[0]); // MOVABS $vwaddrl,%r14
	addbyte(0x49); addbyte(0xbd); addptr64(&vraddrl[0]); // MOVABS $vraddrl,%r13
	addbyte(0x45); addbyte(0x8b); addbyte(0x67); addbyte(15<<2); // MOV R15,%r12d
	block_enter = codeblockpos;
}

static const int canrecompile[256] = {
	1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0, // 00
	0,0,0,0,0,0,0,0,1,0,1,0,1,0,1,0, // 10
	1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0, // 20
	0,0,0,0,0,0,0,0,1,0,1,0,1,0,1,0, // 30

	1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, // 40
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 50
	1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, // 60
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 70

	1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, // 80
	1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, // 90
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // a0
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // b0

	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // c0
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // d0
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // e0
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // f0
};

static void
genstoreimm(int reg, uint32_t val)
{
	addbyte(0x41); addbyte(0xc7); addbyte(0x47); addbyte(reg<<2); addlong(val); // MOVL $val,R{reg}
}

static void
gen_load_reg(int reg, int x86reg)
{
	if (reg == 15) {
		addbyte(0x44); addbyte(0x89); addbyte(0xe0 | x86reg); // MOV %r12d,%{x86reg}
	} else {
		addbyte(0x41); addbyte(0x8b); addbyte(0x47 | (x86reg << 3)); addbyte(reg<<2); // MOV R{reg},%{x86reg}
	}
}

static void
gen_save_reg(int reg, int x86reg)
{
	if (reg == 15) {
		addbyte(0x41); addbyte(0x89); addbyte(0xc4 | (x86reg << 3)); // MOV %{x86reg},%r12d
	} else {
		addbyte(0x41); addbyte(0x89); addbyte(0x47 | (x86reg << 3)); addbyte(reg<<2); // MOV %{x86reg},R{reg}
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
		addbyte(0x41); addbyte(0x03|op); addbyte(0x47); addbyte(RN<<2); // OP RN,%eax
		gen_save_reg(RD, EAX);
	}
}

static void
gen_data_proc_imm(uint32_t opcode, uint8_t op, uint32_t imm)
{
	if (RN == RD) {
		// Can use RMW instruction
		if (imm & ~0x7f) {
			addbyte(0x41); addbyte(0x81); addbyte(0x47|op); addbyte(RD<<2); addlong(imm); // OPL $imm,RD
		} else {
			addbyte(0x41); addbyte(0x83); addbyte(0x47|op); addbyte(RD<<2); addbyte(imm); // OPL $imm,RD
		}
	} else {
		// Load/modify/store
		gen_load_reg(RN, EAX);
		if (RN == 15) {
			addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
		}
		addbyte(0x05|op); addlong(imm); // OP $imm,%eax
		gen_save_reg(RD, EAX);
	}
}

static void
gen_test_armirq(void)
{
	addbyte(0x41); addbyte(0xf7); addbyte(0x47); addbyte(offsetof(ARMState, event)); addlong(0x40); // TESTL $0x40,arm.event
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
	addbyte(0x89); addbyte(0xdf); // MOV %ebx,%edi
	addbyte(0xc1); addbyte(0xea); addbyte(12); // SHR $12,%edx
	addbyte(0x83); addbyte(0xe7); addbyte(0xfc); // AND $0xfffffffc,%edi
	addbyte(0x49); addbyte(0x8b); addbyte(0x54); addbyte(0xd5); addbyte(0); // MOV (%r13,%rdx,8),%rdx
	addbyte(0xf6); addbyte(0xc2); addbyte(1); // TEST $1,%dl
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x8b); addbyte(0x04); addbyte(0x3a); // MOV (%rdx,%rdi),%eax
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	// .notinbuffer
	gen_x86_jump_here(jump_notinbuffer);
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
	addbyte(0x89); addbyte(0xdf); // MOV %ebx,%edi
	addbyte(0xc1); addbyte(0xea); addbyte(12); // SHR $12,%edx
	addbyte(0x49); addbyte(0x8b); addbyte(0x54); addbyte(0xd5); addbyte(0); // MOV (%r13,%rdx,8),%rdx
	addbyte(0xf6); addbyte(0xc2); addbyte(1); // TEST $1,%dl
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x0f); addbyte(0xb6); addbyte(0x04); addbyte(0x3a); // MOVZB (%rdx,%rdi),%eax
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	// .notinbuffer
	gen_x86_jump_here(jump_notinbuffer);
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
 *	%esi	data
 */
static void
genstr(void)
{
	int jump_nextbit, jump_notinbuffer;

	addbyte(0x89); addbyte(0xda); // MOV %ebx,%edx
	addbyte(0x89); addbyte(0xdf); // MOV %ebx,%edi
	addbyte(0xc1); addbyte(0xea); addbyte(12); // SHR $12,%edx
	addbyte(0x83); addbyte(0xe7); addbyte(0xfc); // AND $0xfffffffc,%edi
	addbyte(0x49); addbyte(0x8b); addbyte(0x14); addbyte(0xd6); // MOV (%r14,%rdx,8),%rdx
	addbyte(0xf6); addbyte(0xc2); addbyte(3); // TEST $3,%dl
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x89); addbyte(0x34); addbyte(0x3a); // MOV %esi,(%rdx,%rdi)
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	// .notinbuffer
	gen_x86_jump_here(jump_notinbuffer);
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
 *	%esi	data
 */
static void
genstrb(void)
{
	int jump_nextbit, jump_notinbuffer;

	addbyte(0x89); addbyte(0xda); // MOV %ebx,%edx
	addbyte(0x89); addbyte(0xdf); // MOV %ebx,%edi
	addbyte(0xc1); addbyte(0xea); addbyte(12); // SHR $12,%edx
	addbyte(0x49); addbyte(0x8b); addbyte(0x14); addbyte(0xd6); // MOV (%r14,%rdx,8),%rdx
	addbyte(0xf6); addbyte(0xc2); addbyte(3); // TEST $3,%dl
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x40); addbyte(0x88); addbyte(0x34); addbyte(0x3a); // MOV %sil,(%rdx,%rdi)
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	// .notinbuffer
	gen_x86_jump_here(jump_notinbuffer);
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
 *	%esi	addr (aligned)
 *	%edx	writeback
 *
 * @param opcode Opcode of instruction being emulated
 * @param offset Offset of transfer (transfer size)
 */
static void
gen_arm_ldm_stm_decrement(uint32_t opcode, uint32_t offset)
{
	gen_load_reg(RN, ESI);
	addbyte(0x83); addbyte(0xee); addbyte(offset); // SUB $offset,%esi
	addbyte(0x89); addbyte(0xf2); // MOV %esi,%edx
	if (!(opcode & (1u << 24))) {
		// Decrement After
		addbyte(0x83); addbyte(0xc6); addbyte(4); // ADD $4,%esi
	}

	// Align addr
	addbyte(0x83); addbyte(0xe6); addbyte(0xfc); // AND $0xfffffffc,%esi
}

/**
 * Generate code to calculate the address and writeback values for a LDM/STM
 * increment.
 *
 * Register usage:
 *	%esi	addr (aligned)
 *	%edx	writeback
 *
 * @param opcode Opcode of instruction being emulated
 * @param offset Offset of transfer (transfer size)
 */
static void
gen_arm_ldm_stm_increment(uint32_t opcode, uint32_t offset)
{
	gen_load_reg(RN, ESI);
	addbyte(0x89); addbyte(0xf2); // MOV %esi,%edx
	addbyte(0x83); addbyte(0xc2); addbyte(offset); // ADD $offset,%edx
	if (opcode & (1u << 24)) {
		// Increment Before
		addbyte(0x83); addbyte(0xc6); addbyte(4); // ADD $4,%esi
	}

	// Align addr
	addbyte(0x83); addbyte(0xe6); addbyte(0xfc); // AND $0xfffffffc,%esi
}

/**
 * Generate code to call a LDM/STM helper function.
 *
 * Requires %esi and %edx to contain values for 2nd and 3rd arguments.
 *
 * @param opcode    Opcode of instruction being emulated
 * @param helper_fn Pointer to function to call
 */
static void
gen_call_ldm_stm_helper(uint32_t opcode, const void *helper_fn)
{
	addbyte(0xbf); addlong(opcode); // MOV $opcode,%edi (argument 1)

	addbyte(0x45); addbyte(0x89); addbyte(0x67); addbyte(15<<2); // MOV %r12d,R15
	gen_x86_call(helper_fn);
	addbyte(0x45); addbyte(0x8b); addbyte(0x67); addbyte(15<<2); // MOV R15,%r12d

	gen_test_armirq();
}

/**
 * Generate code to perform a Store Multiple register operation when the S flag
 * is clear.
 *
 * Register usage:
 *	%edi	opcode		(1st function call argument)
 *	%esi	addr		(also 2nd function call argument)
 *	%edx	writeback	(also 3rd function call argument)
 *	%eax	data (scratch)
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
	addbyte(0x89); addbyte(0xf0); // MOV %esi,%eax
	addbyte(0x0d); addlong(0xfffffc00); // OR $0xfffffc00,%eax
	addbyte(0x83); addbyte(0xc0); addbyte(offset - 1); // ADD $(offset - 1),%eax
	jump_page_boundary_cross = gen_x86_jump_forward_long(CC_C);

	// TLB lookup
	addbyte(0x89); addbyte(0xf0); // MOV %esi,%eax
	addbyte(0xc1); addbyte(0xe8); addbyte(12); // SHR $12,%eax
	addbyte(0x49); addbyte(0x8b); addbyte(0x04); addbyte(0xc6); // MOV (%r14,%rax,8),%rax
	addbyte(0xa8); addbyte(0x03); // TEST $3,%al
	jump_tlb_miss = gen_x86_jump_forward_long(CC_NZ);

	// Convert TLB Page and Address to Host address
	addbyte(0x48); addbyte(0x01); addbyte(0xc6); // ADD %rax,%rsi

	// Store first register
	mask = 1;
	d = 0;
	for (c = 0; c < 15; c++) {
		if (opcode & mask) {
			gen_load_reg(c, EAX);
			addbyte(0x89); addbyte(0x46); addbyte(d); // MOV %eax,d(%rsi)
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
			addbyte(0x89); addbyte(0x46); addbyte(d); // MOV %eax,d(%rsi)
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
 * Generate code to perform a Load Multiple register operation when the S flag
 * is clear.
 *
 * Register usage:
 *	%edi	opcode		(1st function call argument)
 *	%esi	addr		(also 2nd function call argument)
 *	%edx	writeback	(also 3rd function call argument)
 *	%eax	data (scratch)
 *	%ecx	TLB page
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
	addbyte(0x89); addbyte(0xf0); // MOV %esi,%eax
	addbyte(0x0d); addlong(0xfffffc00); // OR $0xfffffc00,%eax
	addbyte(0x83); addbyte(0xc0); addbyte(offset - 1); // ADD $(offset - 1),%eax
	jump_page_boundary_cross = gen_x86_jump_forward_long(CC_C);

	// TLB lookup
	addbyte(0x89); addbyte(0xf0); // MOV %esi,%eax
	addbyte(0xc1); addbyte(0xe8); addbyte(12); // SHR $12,%eax
	addbyte(0x49); addbyte(0x8b); addbyte(0x44); addbyte(0xc5); addbyte(0x00); // MOV (%r13,%rax,8),%rax
	addbyte(0xa8); addbyte(0x01); // TEST $1,%al
	jump_tlb_miss = gen_x86_jump_forward_long(CC_NZ);

	// Convert TLB Page and Address to Host address
	addbyte(0x48); addbyte(0x01); addbyte(0xc6); // ADD %rax,%rsi

	// Perform Writeback (if requested)
	if ((opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	// Load registers
	mask = 1;
	d = 0;
	for (c = 0; c < 16; c++) {
		if (opcode & mask) {
			addbyte(0x8b); addbyte(0x46); addbyte(d); // MOV d(%rsi),%eax
			if (c == 15) {
				addbyte(0x41); addbyte(0x8b); addbyte(0x4f); addbyte(offsetof(ARMState, r15_mask)); // MOV arm.r15_mask,%ecx
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

static int
recompile(uint32_t opcode, uint32_t *pcpsr)
{
	uint32_t rhs;
	uint32_t offset;

	NOT_USED(pcpsr);

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
			addbyte(0x41); addbyte(0xf7); addbyte(0x67); addbyte(MULRS<<2); // MULL Rs
			gen_save_reg(MULRD, EAX);
			break;
		}
		if (RD == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_AND, 0);
		break;

	case 0x02: // EOR reg
		if ((opcode & 0xf0) == 0x90) {
			// MLA
			if (MULRD == MULRM) {
				return 0;
			}
			gen_load_reg(MULRM, EAX);
			addbyte(0x41); addbyte(0xf7); addbyte(0x67); addbyte(MULRS<<2); // MULL Rs
			addbyte(0x41); addbyte(0x03); addbyte(0x47); addbyte(MULRN<<2); // ADD Rn,%eax
			gen_save_reg(MULRD, EAX);
			break;
		}
		if (RD == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_XOR, 0);
		break;

	case 0x04: // SUB reg
		if (RD == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_SUB, 1);
		break;

	case 0x08: // ADD reg
		if (arm.arch_v4 && (opcode & 0xf0) == 0x90) {
			// UMULL
			gen_load_reg(MULRM, EAX);
			addbyte(0x41); addbyte(0xf7); addbyte(0x67); addbyte(MULRS<<2); // MULL Rs
			gen_save_reg(MULRN, EAX);
			gen_save_reg(MULRD, EDX);
			break;
		}
		if (RD == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_ADD, 0);
		break;

	case 0x0a: // ADC reg
		// Currently not used
		if (RD == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_load_reg(15, ECX);
		addbyte(0xc1); addbyte(0xe1); addbyte(3); // SHL $3,%ecx - puts ARM carry into x64 carry
		gen_data_proc_reg(opcode, X86_OP_ADC, 0);
		break;

	case 0x18: // ORR reg
		if (RD == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_data_proc_reg(opcode, X86_OP_OR, 0);
		break;

	case 0x1a: // MOV reg
		if (RD == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x1c: // BIC reg
		if (RD == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0xf7); addbyte(0xd0); // NOT %eax
		gen_data_proc_reg(opcode, X86_OP_AND, 0);
		break;

	case 0x1e: // MVN reg
		if (RD == 15) return 0;
		if (!generate_shift(opcode)) {
			return 0;
		}
		addbyte(0xf7); addbyte(0xd0); // NOT %eax
		gen_save_reg(RD, EAX);
		break;

	case 0x20: // AND imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_AND, rhs);
		break;

	case 0x22: // EOR imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_XOR, rhs);
		break;

	case 0x24: // SUB imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_SUB, rhs);
		break;

	case 0x28: // ADD imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_ADD, rhs);
		break;

	case 0x2a: // ADC imm
		// Currently not used
		if (RD == 15) return 0;
		gen_load_reg(15, ECX);
		addbyte(0xc1); addbyte(0xe1); addbyte(3); // SHL $3,%ecx - puts ARM carry into x64 carry
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_ADC, rhs);
		break;

	case 0x38: // ORR imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_OR, rhs);
		break;

	case 0x3a: // MOV imm
		if (RD == 15) return 0;
		rhs = arm_imm(opcode);
		genstoreimm(RD, rhs);
		break;

	case 0x3c: // BIC imm
		if (RD == 15) return 0;
		rhs = ~arm_imm(opcode);
		gen_data_proc_imm(opcode, X86_OP_AND, rhs);
		break;

	case 0x3e: // MVN imm
		if (RD == 15) return 0;
		rhs = ~arm_imm(opcode);
		genstoreimm(RD, rhs);
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
			gen_x86_mov_reg32_stack(EAX, 0);
		}
		gen_load_reg(RN, EBX);
		gen_load_reg(RD, ESI);
		genstr();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EAX, 0);
			if (opcode & 0x800000) {
				addbyte(0x41); addbyte(0x01); addbyte(0x47); addbyte(RN<<2); // ADD %eax,Rn
			} else {
				addbyte(0x41); addbyte(0x29); addbyte(0x47); addbyte(RN<<2); // SUB %eax,Rn
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x41); addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x47); // ADD
				} else {
					addbyte(0x6f); // SUB
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
			gen_x86_mov_reg32_stack(EAX, 0);
		}
		gen_load_reg(RN, EBX);
		gen_load_reg(RD, ESI);
		genstrb();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EAX, 0);
			if (opcode & 0x800000) {
				addbyte(0x41); addbyte(0x01); addbyte(0x47); addbyte(RN<<2); // ADD %eax,Rn
			} else {
				addbyte(0x41); addbyte(0x29); addbyte(0x47); addbyte(RN<<2); // SUB %eax,Rn
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x41); addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x47); // ADD
				} else {
					addbyte(0x6f); // SUB
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
			gen_x86_mov_reg32_stack(EAX, 0);
		}
		gen_load_reg(RN, EBX);
		genldr();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EDX, 0);
			if (opcode & 0x800000) {
				addbyte(0x41); addbyte(0x01); addbyte(0x57); addbyte(RN<<2); // ADD %edx,Rn
			} else {
				addbyte(0x41); addbyte(0x29); addbyte(0x57); addbyte(RN<<2); // SUB %edx,Rn
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x41); addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x47); // ADD
				} else {
					addbyte(0x6f); // SUB
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
			gen_x86_mov_reg32_stack(EAX, 0);
		}
		gen_load_reg(RN, EBX);
		genldrb();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EDX, 0);
			if (opcode & 0x800000) {
				addbyte(0x41); addbyte(0x01); addbyte(0x57); addbyte(RN<<2); // ADD %edx,Rn
			} else {
				addbyte(0x41); addbyte(0x29); addbyte(0x57); addbyte(RN<<2); // SUB %edx,Rn
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x41); addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x47); // ADD
				} else {
					addbyte(0x6f); // SUB
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
		gen_load_reg(RD, ESI);
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
		gen_load_reg(RD, ESI);
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

	case 0xa0: case 0xa1: case 0xa2: case 0xa3: // B
	case 0xa4: case 0xa5: case 0xa6: case 0xa7:
	case 0xa8: case 0xa9: case 0xaa: case 0xab:
	case 0xac: case 0xad: case 0xae: case 0xaf:
		offset = (opcode << 8);
		offset = (uint32_t) ((int32_t) offset >> 6);
		offset += 4;
		if (((PC + offset) & 0xfc000000) == 0 || arm.r15_mask == 0xfffffffc) {
			addbyte(0x41); addbyte(0x81); addbyte(0xc4); addlong(offset); // ADD $offset,%r12d
		} else {
			gen_load_reg(15, EAX);
			addbyte(0x89); addbyte(0xc2); // MOV %eax,%edx
			addbyte(0x05); addlong(offset); // ADD $offset,%eax
			addbyte(0x81); addbyte(0xe2); addlong(0xfc000003); // AND $0xfc000003,%edx
			addbyte(0x25); addlong(0x03fffffc); // AND $0x03fffffc,%eax
			addbyte(0x09); addbyte(0xd0); // OR %edx,%eax
			gen_save_reg(15, EAX);
		}
		blockend = 1;
		break;

	case 0xb0: case 0xb1: case 0xb2: case 0xb3: // BL
	case 0xb4: case 0xb5: case 0xb6: case 0xb7:
	case 0xb8: case 0xb9: case 0xba: case 0xbb:
	case 0xbc: case 0xbd: case 0xbe: case 0xbf:
		offset = (opcode << 8);
		offset = (uint32_t) ((int32_t) offset >> 6);
		offset += 4;
		gen_load_reg(15, EAX);
		addbyte(0x83); addbyte(0xe8); addbyte(0x04); // SUB $4,%eax
		if (((PC + offset) & 0xfc000000) == 0 || arm.r15_mask == 0xfffffffc) {
			addbyte(0x41); addbyte(0x81); addbyte(0xc4); addlong(offset); // ADD $offset,%r12d
			gen_save_reg(14, EAX);
		} else {
			gen_save_reg(14, EAX);
			addbyte(0x89); addbyte(0xc2); // MOV %eax,%edx
			addbyte(0x83); addbyte(0xc0); addbyte(4); // ADD $4,%eax
			addbyte(0x81); addbyte(0xe2); addlong(0xfc000003); // AND $0xfc000003,%edx
			addbyte(0x05); addlong(offset); // ADD $offset,%eax
			addbyte(0x25); addlong(0x03fffffc); // AND $0x03fffffc,%eax
			addbyte(0x09); addbyte(0xd0); // OR %edx,%eax
			gen_save_reg(15, EAX);
		}
		blockend = 1;
		break;

	default:
		return 0;
	}
	lastrecompiled = 1;
	if (lastjumppos != 0) {
		gen_x86_jump_here_long(lastjumppos);
	}
	return 1;
}

void
generatecall(OpFn addr, uint32_t opcode, uint32_t *pcpsr)
{
	lastrecompiled = 0;

	if (canrecompile[(opcode >> 20) & 0xff]) {
		if (recompile(opcode, pcpsr)) {
			return;
		}
	}

	addbyte(0xbf); addlong(opcode); // MOV $opcode,%edi
	addbyte(0x45); addbyte(0x89); addbyte(0x67); addbyte(15<<2); // MOV %r12d,R15
	gen_x86_call(addr);
	addbyte(0x45); addbyte(0x8b); addbyte(0x67); addbyte(15<<2); // MOV R15,%r12d

	if (!flaglookup[opcode >> 28][(*pcpsr) >> 28] && (opcode & 0xe000000) == 0xa000000) {
		// rpclog("Carrying on - %d\n", pcinc);
		// generateupdatepc();
		if (pcinc != 0) {
			addbyte(0x41); addbyte(0x83); addbyte(0xc4); addbyte(pcinc); // ADD $pcinc,%r12d
			// pcinc = 0;
		}
		gen_x86_jump(CC_ALWAYS, 0);
	}
	if (lastjumppos != 0) {
		gen_x86_jump_here_long(lastjumppos);
	}
}

void
generateupdatepc(void)
{
	if (pcinc != 0) {
		addbyte(0x41); addbyte(0x83); addbyte(0xc4); addbyte(pcinc); // ADD $pcinc,%r12d
		pcinc = 0;
	}
}

void
generateupdateinscount(void)
{
	if (tempinscount != 0) {
		if (tempinscount > 127) {
			addbyte(0x81); addbyte(0x05); addrip_long(&inscount, tempinscount); // ADDL $tempinscount,inscount(%rip)
		} else {
			addbyte(0x83); addbyte(0x05); addrip_byte(&inscount, (uint8_t) tempinscount); // ADDL $tempinscount,inscount(%rip)
		}
		tempinscount = 0;
	}
}

void
generatepcinc(void)
{
	lastjumppos = 0;
	tempinscount++;
	pcinc += 4;
	if (pcinc == 124) {
		generateupdatepc();
	}
	if (codeblockpos >= 1200) {
		blockend = 1;
	}
}

void
endblock(uint32_t opcode)
{
	NOT_USED(opcode);

	generateupdatepc();
	generateupdateinscount();

	addbyte(0x83); addbyte(0x2d); addrip_byte(&linecyc, 1); // SUBL $1,linecyc(%rip)
	gen_x86_jump(CC_S, 0);

	addbyte(0x41); addbyte(0xf7); addbyte(0x47); addbyte(offsetof(ARMState, event)); addlong(0xff); // TESTL $0xff,arm.event
	gen_x86_jump(CC_NZ, 0);

	addbyte(0x48); addbyte(0x8d); addbyte(0x0d); addrip(codeblockpc); // LEA codeblockpc(%rip),%rcx
	gen_load_reg(15, EAX);
	addbyte(0x83); addbyte(0xe8); addbyte(8); // SUB $8,%eax
	addbyte(0x48); addbyte(0x8d); addbyte(0x1d); addrip(codeblocknum); // LEA codeblocknum(%rip),%rbx
	addbyte(0x89); addbyte(0xc2); // MOV %eax,%edx
	//if (arm.r15_mask != 0xfffffffc) {
		addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
	//}
	addbyte(0x4c); addbyte(0x8d); addbyte(0x05); addrip(codeblockaddr); // LEA codeblockaddr(%rip),%r8
	addbyte(0x81); addbyte(0xe2); addlong(0x1fffc); // AND $0x1fffc,%edx
	addbyte(0x3b); addbyte(0x04); addbyte(0x11); // CMP (%rcx,%rdx),%eax
	gen_x86_jump(CC_NE, 0);

	addbyte(0x8b); addbyte(0x04); addbyte(0x13); // MOV (%rbx,%rdx),%eax
	addbyte(0x49); addbyte(0x8b); addbyte(0x04); addbyte(0xc0); // MOV (%r8,%rax,8),%rax

	// Jump to next block bypassing function prologue
	addbyte(0x48); addbyte(0x83); addbyte(0xc0); addbyte(block_enter); // ADD $block_enter,%rax
	addbyte(0xff); addbyte(0xe0); // JMP *%rax
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
		if (pcpsr == &arm.reg[15]) {
			addbyte(0x41); addbyte(0xf7); addbyte(0xc4); addlong(0x40000000); // TEST $ZFLAG,%r12d
		} else {
			addbyte(0xf6); addbyte(0x05); addrip_byte(((char *) pcpsr) + 3, 0x40); // TESTB $0x40,(pcpsr+3)(%rip)
		}
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	case 2: // CS
	case 3: // CC
		if (pcpsr == &arm.reg[15]) {
			addbyte(0x41); addbyte(0xf7); addbyte(0xc4); addlong(0x20000000); // TEST $CFLAG,%r12d
		} else {
			addbyte(0xf6); addbyte(0x05); addrip_byte(((char *) pcpsr) + 3, 0x20); // TESTB $0x20,(pcpsr+3)(%rip)
		}
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	case 4: // MI
	case 5: // PL
		if (pcpsr == &arm.reg[15]) {
			addbyte(0x45); addbyte(0x09); addbyte(0xe4); // OR %r12d,%r12d
			cond = ((opcode >> 28) & 1) ? CC_S : CC_NS;
		} else {
			addbyte(0xf6); addbyte(0x05); addrip_byte(((char *) pcpsr) + 3, 0x80); // TESTB $0x80,(pcpsr+3)(%rip)
			cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		}
		break;
	case 6: // VS
	case 7: // VC
		if (pcpsr == &arm.reg[15]) {
			addbyte(0x41); addbyte(0xf7); addbyte(0xc4); addlong(0x10000000); // TEST $VFLAG,%r12d
		} else {
			addbyte(0xf6); addbyte(0x05); addrip_byte(((char *) pcpsr) + 3, 0x10); // TESTB $0x10,(pcpsr+3)(%rip)
		}
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	default:
		addbyte(0x48); addbyte(0x8d); addbyte(0x1d); addrip(&flaglookup[opcode >> 28][0]); // LEA flaglookup(%rip),%rbx
		if (pcpsr == &arm.reg[15]) {
			gen_load_reg(15, EAX);
		} else {
			addbyte(0x8b); addbyte(0x05); addrip(pcpsr); // MOV pcpsr(%rip),%eax
		}
		addbyte(0xc1); addbyte(0xe8); addbyte(28); // SHR $28,%eax
		addbyte(0x80); addbyte(0x3c); addbyte(0x03); addbyte(0); // CMPB $0,(%rbx,%rax)
		cond = CC_E;
		break;
	}
	lastjumppos = gen_x86_jump_forward_long(cond);
}

void
generateirqtest(void)
{
	if (lastrecompiled) {
		lastrecompiled = 0;
		return;
	}

	addbyte(0x85); addbyte(0xc0); // TEST %eax,%eax
	gen_x86_jump(CC_NE, 0);
	if (lastjumppos != 0) {
		gen_x86_jump_here_long(lastjumppos);
	}
}
