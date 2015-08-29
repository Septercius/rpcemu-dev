#ifndef __ARM__
#define __ARM__

#include "rpcemu.h"

typedef struct {
	uint32_t	reg[18];
	uint32_t	mode;
	uint32_t	mmask;

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
} ARMState;

typedef void (*OpFn)(uint32_t opcode);

extern void updatemode(uint32_t m);
extern void resetcodeblocks(void);
extern void initcodeblocks(void);
extern void generatepcinc(void);
extern void generateupdatepc(void);
extern void generateupdateinscount(void);
extern void generateflagtestandbranch(uint32_t opcode, uint32_t *pcpsr);
extern void generatecall(OpFn addr, uint32_t opcode, uint32_t *pcpsr);
extern void generateirqtest(void);
extern void endblock(uint32_t opcode, uint32_t *pcpsr);
extern void initcodeblock(uint32_t l);

extern uint32_t *usrregs[16];
extern int armirq; //,armfiq;
extern int cpsr;

#define PC ((arm.reg[15] - 8) & r15mask)

extern uint32_t r15mask;

extern void arm_init(void);
extern void resetarm(CPUModel cpu_model);
extern void execarm(int cycles);
extern void dumpregs(void);
extern void exception(int mmode, uint32_t address, int diff);

extern ARMState arm;

extern int databort,prefabort;
extern int prog32;
extern int blockend;

extern int lastflagchange;

#define RD ((opcode>>12)&0xF)
#define RN ((opcode>>16)&0xF)
#define RM (opcode&0xF)

#define MULRD ((opcode>>16)&0xF)
#define MULRN ((opcode>>12)&0xF)
#define MULRS ((opcode>>8)&0xF)
#define MULRM (opcode&0xF)

extern uint32_t rotatelookup[4096];
#define rotate2(v) rotatelookup[v&4095]

#define countbits(c) countbitstable[c]
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

#endif //__ARM__
