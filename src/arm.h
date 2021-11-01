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

#ifndef __ARM__
#define __ARM__

#include "rpcemu.h"

typedef struct {
	uint32_t	reg[17];
	uint32_t	mode;
	uint32_t	mmask;
	uint32_t	r15_mask;

	uint32_t	event;

	/* Banked registers */
	uint32_t	user_reg[15];
	uint32_t	fiq_reg[15];
	uint32_t	irq_reg[2];
	uint32_t	super_reg[2];
	uint32_t	abort_reg[2];
	uint32_t	undef_reg[2];
	uint32_t	spsr[16];

	uint32_t	r15_diff;
	uint8_t		abort_base_restored;
	uint8_t		stm_writeback_at_end;
	uint8_t		arch_v4;
} ARMState;

typedef int (*OpFn)(uint32_t opcode);

extern void updatemode(uint32_t m);
extern void resetcodeblocks(void);
extern void initcodeblocks(void);
extern void generatepcinc(void);
extern void generateupdatepc(void);
extern void generateupdateinscount(void);
extern void generateflagtestandbranch(uint32_t opcode, uint32_t *pcpsr);
extern void generatecall(OpFn addr, uint32_t opcode, uint32_t *pcpsr);
extern void generateirqtest(void);
extern void endblock(uint32_t opcode);
extern void initcodeblock(uint32_t l);

extern uint32_t *usrregs[16];
extern int cpsr;
extern uint32_t pccache;

#define PC	((arm.reg[15] - 8) & arm.r15_mask)

extern int arm_is_dynarec(void); 
extern void arm_init(void);
extern void arm_reset(CPUModel cpu_model);
extern int arm_exec(void);
extern void arm_dump(void);
extern void exception(uint32_t mmode, uint32_t address, uint32_t diff);
extern void set_memory_executable(void *ptr, size_t len);

extern ARMState arm;

extern int prog32;
extern int blockend;
extern int linecyc;

extern int lastflagchange;

#define RD ((opcode>>12)&0xF)
#define RN ((opcode>>16)&0xF)
#define RM (opcode&0xF)

#define MULRD ((opcode>>16)&0xF)
#define MULRN ((opcode>>12)&0xF)
#define MULRS ((opcode>>8)&0xF)
#define MULRM (opcode&0xF)

extern int countbitstable[65536];

#define NFLAG 0x80000000
#define ZFLAG 0x40000000
#define CFLAG 0x20000000
#define VFLAG 0x10000000

#define USER       0
#define FIQ        1
#define IRQ        2
#define SUPERVISOR 3
#define ABORT      7
#define UNDEFINED  11
#define SYSTEM     15

static inline uint32_t
GETADDR(uint32_t r)
{
	if (r == 15) {
		return arm.reg[15] & arm.r15_mask;
	} else {
		return arm.reg[r];
	}
}

/**
 * Generate an Undefined Instruction exception.
 */
static inline void
arm_exception_undefined(void)
{
	exception(UNDEFINED, 8, 4);
}

#endif //__ARM__
