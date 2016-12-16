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

#include "rpcemu.h"

#if defined i386 || defined __i386 || defined __i386__ || defined _X86_ || defined WIN32 || defined _WIN32 || defined _WIN32

#include <assert.h>
#include <stdint.h>
#include "codegen_x86.h"
#include "mem.h"
#include "arm.h"
#include "arm_common.h"

#if defined __linux__ || defined __MACH__
#include <sys/mman.h>
#include <unistd.h>
#endif

void generateupdatepc(void);
int linecyc;

unsigned char rcodeblock[BLOCKS][1792+512+64];
static const void *codeblockaddr[BLOCKS];
uint32_t codeblockpc[0x8000];
static unsigned char codeblockpresent[0x10000];
int codeblocknum[0x8000] = {0};

static int flagsdirty = 0;
//#define BLOCKS 4096
//#define HASH(l) ((l>>3)&0x3FFF)
int blockend = 0;
static int blocknum;//,blockcount;
static int tempinscount;

static int codeblockpos = 0;

static unsigned char lahftable[256], lahftablesub[256];

static void gen_load_reg(int reg, int x86reg);
static void gen_save_reg(int reg, int x86reg);

static int blockpoint = 0, blockpoint2 = 0;
static uint32_t blocks[BLOCKS];
static int pcinc = 0;
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
	*((uint32_t *) &rcodeblock[blockpoint2][codeblockpos]) = a;
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
#if defined __linux__ || defined __MACH__
	void *start;
	size_t len;
	long pagesize = sysconf(_SC_PAGESIZE);
	long pagemask = ~(pagesize - 1);
#endif
	/* Clear all blocks */
	memset(codeblockpc, 0xff, sizeof(codeblockpc));
	memset(blocks, 0xff, sizeof(blocks));
	for (c = 0; c < BLOCKS; c++) {
		codeblockaddr[c] = &rcodeblock[c][0];
	}
	blockpoint = 0;

        for (c=0;c<256;c++)
        {
                lahftable[c]=0;
                if (c&1) lahftable[c]|=0x20; /*C flag*/
                lahftable[c]|=(c&0xC0);      /*Z and N flags*/
                lahftablesub[c]=lahftable[c]^0x20;
        }

#if defined __linux__ || defined __MACH__
	/* Set memory pages containing rcodeblock[]s executable -
	   necessary when NX/XD feature is active on CPU(s) */
	start = (void *)((long)rcodeblock & pagemask);
	len = (sizeof rcodeblock + pagesize) & pagemask;
	if (mprotect(start, len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
		perror("mprotect");
		exit(1);
	}
#endif
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
        int c,d;
        if (!codeblockpresent[a&0xFFFF]) return;
        codeblockpresent[a&0xFFFF]=0;
//        a>>=10;
d=HASH(a<<12);
        for (c=0;c<0x400;c++)
        {
                if ((codeblockpc[c+d]>>12)==a) codeblockpc[c+d]=0xFFFFFFFF;
        }
/*        codeblockpc[hash][0]=0xFFFFFFFF;
        codeblockpc[hash][1]=0xFFFFFFFF;
        codeblockpc[hash][2]=0xFFFFFFFF;*/
}

static uint32_t currentblockpc, currentblockpc2;

void
initcodeblock(uint32_t l)
{
        codeblockpresent[(l>>12)&0xFFFF]=1;
        tempinscount=0;
//        rpclog("Initcodeblock %08X\n",l);
        blockpoint++;
        blockpoint&=(BLOCKS-1);
        if (blocks[blockpoint]!=0xFFFFFFFF)
        {
//                rpclog("Chucking out block %08X %i %03X\n",blocks[blockpoint],blocks[blockpoint]>>24,blocks[blockpoint]&0xFFF);
                codeblockpc[blocks[blockpoint]&0x7FFF]=0xFFFFFFFF;
                codeblocknum[blocks[blockpoint]&0x7FFF]=0xFFFFFFFF;
        }
        blocknum=HASH(l);
//        blockcount=0;//codeblockcount[blocknum];
//        codeblockcount[blocknum]++;
//        if (codeblockcount[blocknum]==3) codeblockcount[blocknum]=0;
        codeblockpos=0;
        codeblockpc[blocknum]=l;
        codeblocknum[blocknum]=blockpoint;
        blocks[blockpoint]=blocknum;
        blockpoint2=blockpoint;
        
	/* Block Epilogue */
	addbyte(0x83); /* ADD $12,%esp */
	addbyte(0xc4);
	addbyte(0x0c);
	/* Restore registers */
	gen_x86_pop_reg(EBX);
	gen_x86_pop_reg(ESI);
	gen_x86_pop_reg(EDI);
	gen_x86_leave();
	gen_x86_ret();

        addbyte(0xE9); /*JMP end*/
        addlong(0); /*Don't know where end is yet - see endblock()*/

	/* Block Prologue */
	assert(codeblockpos <= BLOCKSTART);
	codeblockpos = BLOCKSTART;
	/* Set up a stack frame and preserve registers that are callee-saved */
	gen_x86_push_reg(EBP);
	addbyte(0x89); addbyte(0xe5); /* MOV %esp,%ebp */
	gen_x86_push_reg(EDI);
	gen_x86_push_reg(ESI);
	gen_x86_push_reg(EBX);
	/* Align stack to a multiple of 16 bytes - required by Mac OS X */
	addbyte(0x83); /* SUB $12,%esp */
	addbyte(0xec);
	addbyte(0x0c);
	addbyte(0xbe); addptr(&arm); // MOV $(&arm),%esi
	block_enter = codeblockpos;
	currentblockpc = arm.reg[15] & arm.r15_mask;
	currentblockpc2 = PC;
	flagsdirty = 0;
}

void
removeblock(void)
{
        codeblockpc[blocknum]=0xFFFFFFFF;
        codeblocknum[blocknum]=0xFFFFFFFF;
}

int lastflagchange=0;

static const int recompileinstructions[256] = {
        1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0, //00
        0,1,0,1,0,1,0,0,1,1,1,1,1,1,1,1, //10
        1&0,1,1,1,1,1,0,0,1,1,0,0,0,0,0,0, //20
        0,1,0,1,0,1,0,0,1,1,1,1,1,1,1,1, //30

        1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, //40
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //50
        1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, //60
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //70

        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //80
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //90
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //A0
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, //B0

        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, //C0
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, //D0
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, //E0
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  //F0
};

static void
gen_load_reg(int reg, int x86reg)
{
	if (reg != 0) {
		addbyte(0x8B); addbyte(0x46 | (x86reg << 3)); addbyte(reg*4);
	} else {
		addbyte(0x8B); addbyte(0x06 | (x86reg << 3));
	}
}

static void
gen_save_reg(int reg, int x86reg)
{
	if (reg != 0) {
		addbyte(0x89); addbyte(0x46 | (x86reg << 3)); addbyte(reg*4);
	} else {
		addbyte(0x89); addbyte(0x06 | (x86reg << 3));
	}
}

static void
generatedataproc(uint32_t opcode, unsigned char dataop, uint32_t templ)
{
//        #if 0
        if (RN==RD)
        {
                addbyte(0x81); /*ORRL $dat,(addr)*/
                addbyte(0x05|dataop);
                addptr(&arm.reg[RD]);
                addlong(templ);
        }
        else
        {
//                #endif
                gen_load_reg(RN, EAX);
                if (RN == 15 && arm.r15_mask != 0xfffffffc) {
                        addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
                }
                if (!(templ&~0x7F))
                {
                        addbyte(0x83); /*ORRL $8,%eax*/
                        addbyte(0xC0|dataop);
                        addbyte(templ);
                }
                else
                {
                        addbyte(0x81); /*ORRL $8,%eax*/
                        addbyte(0xC0|dataop);
                        addlong(templ);
                }
                gen_save_reg(RD, EAX);
        }
}

static void
generatedataprocS(uint32_t opcode, unsigned char dataop, uint32_t templ)
{
        if (RN==RD)
        {
                addbyte(0x81); /*ORRL $dat,(addr)*/
                addbyte(0x05|dataop);
                addptr(&arm.reg[RD]);
                addlong(templ);
                gen_x86_lahf();
        }
        else
        {
                gen_load_reg(RN, EDX);
                if (RN == 15 && arm.r15_mask != 0xfffffffc) {
                        addbyte(0x81); addbyte(0xe2); addlong(arm.r15_mask); // AND $arm.r15_mask,%edx
                }
                if (!(templ&~0x7F))
                {
                        addbyte(0x83); /*ORRL $8,%edx*/
                        addbyte(0xC2|dataop);
                        addbyte(templ);
                }
                else
                {
                        addbyte(0x81); /*ORRL $8,%edx*/
                        addbyte(0xC2|dataop);
                        addlong(templ);
                }
                gen_x86_lahf();
                gen_save_reg(RD, EDX);
        }
        //gen_x86_lahf();
}

static int
generate_shift(uint32_t opcode)
{
	uint32_t shift_amount;

	if (opcode & 0x10) {
		return 0; /* Can't do register shifts or multiplies */
	}
	if ((opcode & 0xff0) == 0) {
		/* No shift */
		gen_load_reg(RM, EAX);
		return 1;
	}
	shift_amount = (opcode >> 7) & 0x1f;
	switch (opcode & 0x60) {
	case 0x00: /* LSL */
		gen_load_reg(RM, EAX);
		if (shift_amount != 0) {
			addbyte(0xc1); addbyte(0xe0); addbyte(shift_amount); // SHL $shift_amount,%eax
		}
		return 1;
	case 0x20: /* LSR */
		if (shift_amount != 0) {
			gen_load_reg(RM, EAX);
			addbyte(0xc1); addbyte(0xe8); addbyte(shift_amount); // SHR $shift_amount,%eax
		} else {
			addbyte(0x31); addbyte(0xc0); // XOR %eax,%eax
		}
		return 1;
	case 0x40: /* ASR */
		if (shift_amount == 0) {
			shift_amount = 31;
		}
		gen_load_reg(RM, EAX);
		addbyte(0xc1); addbyte(0xf8); addbyte(shift_amount); // SAR $shift_amount,%eax
		return 1;
	default: /* ROR */
		if (shift_amount == 0) {
			/* RRX */
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
        unsigned int temp;
        if (opcode&0x10) return 0; /*Can't do shift by register ATM*/
        if (!(opcode&0xFF0)) /*No shift*/
        {
                addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); /*MOVB *pcpsr,%cl*/
                gen_load_reg(RM, EAX);
                addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $ZFLAG+NFLAG,%cl*/
                return 1;
        }
        temp=(opcode>>7)&31;
        switch (opcode&0x60)
        {
                case 0x00: /*LSL*/
                addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); /*MOVB *pcpsr,%cl*/
                gen_load_reg(RM, EAX);
                if (temp)  
                {
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xE0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                        addbyte(0xC1); addbyte(0xE0); addbyte(temp); /*SHL $temp,%eax*/
                        addbyte(0x73); addbyte(3); /*JNC nocarry*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                }
                else
                {
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xC0); /*AND $ZFLAG+NFLAG,%cl*/
                }
                return 1;
                case 0x20: /*LSR*/
//                return 0;
                if (temp)
                {
                        addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); /*MOVB *pcpsr,%cl*/
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xE0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                        gen_load_reg(RM, EAX);
                        addbyte(0xC1); addbyte(0xE8); addbyte(temp); /*SHR $temp,%eax*/
                        addbyte(0x73); addbyte(3); /*JNC nocarry*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                }
                else
                {
                        return 0;
                        addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); /*MOVB *pcpsr,%cl*/
                        addbyte(0x80); addbyte(0xE1); addbyte(~0xE0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                        addbyte(0xA9); addlong(0x80000000); /*TEST $0x80000000,%eax*/
                        addbyte(0x74); addbyte(3); /*JZ nocarry*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                        addbyte(0x31); addbyte(0xC0); /*XOR %eax,%eax*/
                }
                return 1;
                case 0x40: /*ASR*/
                return 0;
                addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); /*MOVB *pcpsr,%cl*/
                addbyte(0x80); addbyte(0xE1); addbyte(~0xE0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                if (!temp)
                {
                        gen_load_reg(RM, EAX);
                        addbyte(0xA9); addlong(0x80000000); /*TEST $0x80000000,%eax*/
                        addbyte(0x74); addbyte(3); /*JZ nocarry*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                        addbyte(0xC1); addbyte(0xF8); addbyte(31); /*SAR $31,%eax*/
                }
                else
                {
                        gen_load_reg(RM, EAX);
                        addbyte(0xC1); addbyte(0xF8); addbyte(temp); /*SAR $temp,%eax*/
                        addbyte(0x73); addbyte(3); /*JNC nocarry*/
                        addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                }
                return 1;
                case 0x60: /*ROR*/
                return 0;
                if (!temp) break;
                addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); /*MOVB *pcpsr,%cl*/
                gen_load_reg(RM, EAX);
                addbyte(0x80); addbyte(0xE1); addbyte(~0xE0); /*AND $ZFLAG+NFLAG+CFLAG,%cl*/
                addbyte(0xC1); addbyte(0xC8); addbyte(temp); /*ROR $temp,%eax*/
                addbyte(0x73); addbyte(3); /*JNC nocarry*/
                addbyte(0x80); addbyte(0xC9); addbyte(0x20); /*OR $CFLAG,%cl*/
                return 1;
        }
        return 0;
}

static uint32_t
generaterotate(uint32_t opcode, uint32_t *pcpsr, uint8_t mask)
{
        uint32_t temp;

        if (!flagsdirty) {
                addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); /*MOVB *pcpsr,%cl*/
        }
        temp = arm_imm(opcode);
        if (mask!=0xF0)
        {
                if (opcode&0xF00)
                {
                        if (temp&0x80000000) { addbyte(0x80); addbyte(0xC9); addbyte( 0x20); } /*OR $CFLAG,%cl*/
                        else                 { addbyte(0x80); addbyte(0xE1); addbyte(~(0x20|mask)); } /*AND $~CFLAG,%cl*/
                }
        }
        if (!(opcode&0xF00) || (temp&0x80000000) || mask==0xF0) { addbyte(0x80); addbyte(0xE1); addbyte(~mask); } /*AND $~mask,%cl*/
        return temp;
}

static void
generatesetzn(uint32_t opcode, uint32_t *pcpsr)
{
//        addbyte(0x75); addbyte(3); /*JNZ testn*/
//        addbyte(0x80); addbyte(0xC9); addbyte(0x40); /*OR $ZFLAG,%cl*/
        /*.testn*/
//        addbyte(0x79); addbyte(3); /*JNS over*/
//        addbyte(0x80); addbyte(0xC9); addbyte(0x80); /*OR $NFLAG,%cl*/
        /*over*/

        gen_x86_lahf();
        addbyte(0x80); addbyte(0xE4); addbyte(0xC0); /*AND $ZFLAG+NFLAG,%ah*/
        addbyte(0x08); addbyte(0xE1); /*OR %ah,%cl*/

        if ((opcode>>28)==0xE) flagsdirty=1;
        addbyte(0x88); addbyte(0x0d); addptr(((char *) pcpsr) + 3); /*MOV %cl,pcpsr*/
}

static void
generatesetzn2(uint32_t opcode, uint32_t *pcpsr)
{
        gen_x86_lahf();
        addbyte(0x80); addbyte(0xE4); addbyte(0xC0); /*AND $ZFLAG+NFLAG,%ah*/
        addbyte(0x08); addbyte(0xE1); /*OR %ah,%cl*/
        if ((opcode>>28)==0xE) flagsdirty=1;
/*        if ((opcode>>28)==0xE)
           flagsdirty=1;
        else
        {*/
                addbyte(0x88); addbyte(0x0d); addptr(((char *) pcpsr) + 3); /*MOV %cl,pcpsr*/
//        }
}

static void
generatesetznS(uint32_t opcode, uint32_t *pcpsr)
{
        //gen_x86_lahf();
        addbyte(0x80); addbyte(0xE4); addbyte(0xC0); /*AND $ZFLAG+NFLAG,%ah*/
        addbyte(0x08); addbyte(0xE1); /*OR %ah,%cl*/
        addbyte(0x88); addbyte(0x0d); addptr(((char *) pcpsr) + 3); /*MOV %cl,pcpsr*/
        if ((opcode>>28)==0xE) flagsdirty=1;
//        flagsdirty=1;
}

static int lastrecompiled;

static void
gen_test_armirq(void)
{
	addbyte(0xf6); addbyte(0x05); addptr(&armirq); addbyte(0x40); // TESTB $0x40,armirq
	gen_x86_jump(CC_NZ, 0);
}

static void
genldr(void)
{
	int jump_nextbit, jump_notinbuffer;

	addbyte(0x89); addbyte(0xda); /* MOV %ebx,%edx */
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xea); addbyte(12); /* SHR $12,%edx */
	addbyte(0x83); addbyte(0xe0); addbyte(0xfc); // AND $0xfffffffc,%eax
	addbyte(0x8b); addbyte(0x14); addbyte(0x95); addptr(vraddrl); // MOV vraddrl(,%edx,4),%edx
	addbyte(0xf6); addbyte(0xc2); addbyte(1); /* TEST $1,%dl */
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x8b); addbyte(0x04); addbyte(0x02); // MOV (%edx,%eax),%eax
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	/* .notinbuffer */
	gen_x86_jump_here(jump_notinbuffer);
	gen_x86_mov_reg32_stack(EAX, 0);
	gen_x86_call(readmemfl);
	if (arm.abort_base_restored) {
		gen_test_armirq();
	}
	/* .nextbit */
	gen_x86_jump_here(jump_nextbit);
	/* Rotate if load is unaligned */
	addbyte(0x89); addbyte(0xd9); /* MOV %ebx,%ecx */
	addbyte(0xc1); addbyte(0xe1); addbyte(3); /* SHL $3,%ecx */
	addbyte(0xd3); addbyte(0xc8); /* ROR %cl,%eax */
}

static void
genldrb(void)
{
	int jump_nextbit, jump_notinbuffer;

	addbyte(0x89); addbyte(0xda); /* MOV %ebx,%edx */
	addbyte(0xc1); addbyte(0xea); addbyte(12); /* SHR $12,%edx */
	addbyte(0x8b); addbyte(0x14); addbyte(0x95); addptr(vraddrl); // MOV vraddrl(,%edx,4),%edx
	addbyte(0xf6); addbyte(0xc2); addbyte(1); /* TEST $1,%dl */
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x0f); addbyte(0xb6); addbyte(0x04); addbyte(0x1a); /* MOVZB (%edx,%ebx),%eax */
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	/* .notinbuffer */
	gen_x86_jump_here(jump_notinbuffer);
	gen_x86_mov_reg32_stack(EBX, 0);
	gen_x86_call(readmemfb);
	if (arm.abort_base_restored) {
		gen_test_armirq();
	}
	/* .nextbit */
	gen_x86_jump_here(jump_nextbit);
}

static void
genstr(void)
{
	int jump_nextbit, jump_notinbuffer;

	addbyte(0x89); addbyte(0xda); /* MOV %ebx,%edx */
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xea); addbyte(12); /* SHR $12,%edx */
	addbyte(0x83); addbyte(0xe0); addbyte(0xfc); // AND $0xfffffffc,%eax
	addbyte(0x8b); addbyte(0x14); addbyte(0x95); addptr(vwaddrl); // MOV vwaddrl(,%edx,4),%edx
	addbyte(0xf6); addbyte(0xc2); addbyte(3); /* TEST $3,%dl */
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x89); addbyte(0x0c); addbyte(0x02); // MOV %ecx,(%edx,%eax)
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	/* .notinbuffer */
	gen_x86_jump_here(jump_notinbuffer);
	gen_x86_mov_reg32_stack(EAX, 0);
	gen_x86_mov_reg32_stack(ECX, 4);
	gen_x86_call(writememfl);
	if (arm.abort_base_restored) {
		gen_test_armirq();
	}
	/* .nextbit */
	gen_x86_jump_here(jump_nextbit);
}

static void
genstrb(void)
{
	int jump_nextbit, jump_notinbuffer;

	addbyte(0x89); addbyte(0xda); /* MOV %ebx,%edx */
	addbyte(0xc1); addbyte(0xea); addbyte(12); /* SHR $12,%edx */
	addbyte(0x8b); addbyte(0x14); addbyte(0x95); addptr(vwaddrl); // MOV vwaddrl(,%edx,4),%edx
	addbyte(0xf6); addbyte(0xc2); addbyte(3); /* TEST $3,%dl */
	jump_notinbuffer = gen_x86_jump_forward(CC_NZ);
	addbyte(0x88); addbyte(0x0c); addbyte(0x1a); /* MOV %cl,(%edx,%ebx) */
	jump_nextbit = gen_x86_jump_forward(CC_ALWAYS);
	/* .notinbuffer */
	gen_x86_jump_here(jump_notinbuffer);
	gen_x86_mov_reg32_stack(EBX, 0);
	gen_x86_mov_reg32_stack(ECX, 4);
	gen_x86_call(writememfb);
	if (arm.abort_base_restored) {
		gen_test_armirq();
	}
	/* .nextbit */
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
		/* Decrement After */
		addbyte(0x83); addbyte(0xc3); addbyte(4); // ADD $4,%ebx
	}

	/* Align addr */
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
		/* Increment Before */
		addbyte(0x83); addbyte(0xc3); addbyte(4); // ADD $4,%ebx
	}

	/* Align addr */
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

	/* Check if crossing Page boundary */
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0x0d); addlong(0xfffffc00); // OR $0xfffffc00,%eax
	addbyte(0x83); addbyte(0xc0); addbyte(offset - 1); // ADD $(offset - 1),%eax
	jump_page_boundary_cross = gen_x86_jump_forward_long(CC_C);

	/* TLB lookup */
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xe8); addbyte(12); // SHR $12,%eax
	addbyte(0x8b); addbyte(0x04); addbyte(0x85); addptr(vwaddrl); // MOV vwaddrl(,%eax,4),%eax
	addbyte(0xa8); addbyte(0x03); // TEST $3,%al
	jump_tlb_miss = gen_x86_jump_forward_long(CC_NZ);

	/* Convert TLB Page and Address to Host address */
	addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx

	/* Store first register */
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

	/* Perform Writeback (if requested) at end of 2nd cycle */
	if (!arm.stm_writeback_at_end && (opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	/* Store remaining registers */
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

	/* Perform Writeback (if requested) at end of instruction (SA110) */
	if (arm.stm_writeback_at_end && (opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	jump_done = gen_x86_jump_forward(CC_ALWAYS);

	/* Call helper function */
	gen_x86_jump_here_long(jump_page_boundary_cross);
	gen_x86_jump_here_long(jump_tlb_miss);
	gen_call_ldm_stm_helper(opcode, arm_store_multiple);

	/* All done, continue here */
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

	/* Check if crossing Page boundary */
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0x0d); addlong(0xfffffc00); // OR $0xfffffc00,%eax
	addbyte(0x83); addbyte(0xc0); addbyte(offset - 1); // ADD $(offset - 1),%eax
	jump_page_boundary_cross = gen_x86_jump_forward_long(CC_C);

	/* TLB lookup */
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xe8); addbyte(12); // SHR $12,%eax
	addbyte(0x8b); addbyte(0x04); addbyte(0x85); addptr(vwaddrl); // MOV vwaddrl(,%eax,4),%eax
	addbyte(0xa8); addbyte(0x03); // TEST $3,%al
	jump_tlb_miss = gen_x86_jump_forward_long(CC_NZ);

	/* Convert TLB Page and Address to Host address */
	addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx

	/* Store first register */
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

	/* Perform Writeback (if requested) at end of 2nd cycle */
	if (!arm.stm_writeback_at_end && (opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	/* Store remaining registers */
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

	/* Perform Writeback (if requested) at end of instruction (SA110) */
	if (arm.stm_writeback_at_end && (opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	jump_done = gen_x86_jump_forward(CC_ALWAYS);

	/* Call helper function */
	gen_x86_jump_here_long(jump_page_boundary_cross);
	gen_x86_jump_here_long(jump_tlb_miss);
	gen_call_ldm_stm_helper(opcode, arm_store_multiple_s);

	/* All done, continue here */
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

	/* Check if crossing Page boundary */
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0x0d); addlong(0xfffffc00); // OR $0xfffffc00,%eax
	addbyte(0x83); addbyte(0xc0); addbyte(offset - 1); // ADD $(offset - 1),%eax
	jump_page_boundary_cross = gen_x86_jump_forward_long(CC_C);

	/* TLB lookup */
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xe8); addbyte(12); // SHR $12,%eax
	addbyte(0x8b); addbyte(0x04); addbyte(0x85); addptr(vraddrl); // MOV vraddrl(,%eax,4),%eax
	addbyte(0xa8); addbyte(0x01); // TEST $1,%al
	jump_tlb_miss = gen_x86_jump_forward_long(CC_NZ);

	/* Convert TLB Page and Address to Host address */
	addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx

	/* Perform Writeback (if requested) */
	if ((opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	/* Load registers */
	mask = 1;
	d = 0;
	for (c = 0; c < 16; c++) {
		if (opcode & mask) {
			addbyte(0x8b); addbyte(0x43); addbyte(d); // MOV d(%ebx),%eax
			if (c == 15) {
				addbyte(0x8b); addbyte(0x0d); addptr(&arm.r15_mask); // MOV arm.r15_mask,%ecx
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

	/* Call helper function */
	gen_x86_jump_here_long(jump_page_boundary_cross);
	gen_x86_jump_here_long(jump_tlb_miss);
	gen_call_ldm_stm_helper(opcode, arm_load_multiple);

	/* All done, continue here */
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

	/* Check if crossing Page boundary */
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0x0d); addlong(0xfffffc00); // OR $0xfffffc00,%eax
	addbyte(0x83); addbyte(0xc0); addbyte(offset - 1); // ADD $(offset - 1),%eax
	jump_page_boundary_cross = gen_x86_jump_forward_long(CC_C);

	/* TLB lookup */
	addbyte(0x89); addbyte(0xd8); // MOV %ebx,%eax
	addbyte(0xc1); addbyte(0xe8); addbyte(12); // SHR $12,%eax
	addbyte(0x8b); addbyte(0x04); addbyte(0x85); addptr(vraddrl); // MOV vraddrl(,%eax,4),%eax
	addbyte(0xa8); addbyte(0x01); // TEST $1,%al
	jump_tlb_miss = gen_x86_jump_forward_long(CC_NZ);

	/* Convert TLB Page and Address to Host address */
	addbyte(0x01); addbyte(0xc3); // ADD %eax,%ebx

	/* Perform Writeback (if requested) */
	if ((opcode & (1u << 21)) && (RN != 15)) {
		gen_save_reg(RN, EDX);
	}

	/* Perform Load into User Bank */
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

	/* Call helper function */
	gen_x86_jump_here_long(jump_page_boundary_cross);
	gen_x86_jump_here_long(jump_tlb_miss);
	gen_call_ldm_stm_helper(opcode, arm_load_multiple_s);

	/* All done, continue here */
	gen_x86_jump_here(jump_done);
}

static int
recompile(uint32_t opcode, uint32_t *pcpsr)
{
	uint32_t templ;
	uint32_t offset;

	switch ((opcode >> 20) & 0xff) {
	case 0x00: /* AND reg */
		if ((opcode & 0xf0) == 0x90) {
			/* MUL */
			if (MULRD == MULRM) {
				addbyte(0x31); addbyte(0xc0); // XOR %eax,%eax
			} else {
				gen_load_reg(MULRM, EAX);
				addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
				gen_save_reg(MULRD, EAX);
			}
			break;
		}
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x23); addbyte(0x46); addbyte(RN<<2); // AND Rn,%eax
		gen_save_reg(RD, EAX);
		break;

	case 0x01: /* ANDS reg */
		if ((opcode & 0xf0) == 0x90) {
			/* MULS */
			if (!flagsdirty) {
				addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV pcpsr,%cl
			}
			addbyte(0x80); addbyte(0xe1); addbyte(0x3f); // AND $~(NFLAG|ZFLAG),%cl
			if (MULRD == MULRM) {
				addbyte(0x31); addbyte(0xc0); // XOR %eax,%eax
			} else {
				gen_load_reg(MULRM, EAX);
				addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
				gen_save_reg(MULRD, EAX);
			}
			addbyte(0x85); addbyte(0xc0); // TEST %eax,%eax
			generatesetzn(opcode, pcpsr);
			break;
		}
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x23); addbyte(0x46); addbyte(RN<<2); // AND Rn,%eax
		gen_save_reg(RD, EAX);
		generatesetzn(opcode, pcpsr);
		break;

	case 0x02: /* EOR reg */
		if ((opcode & 0xf0) == 0x90) {
			/* MLA */
			if (MULRD == MULRM) {
				addbyte(0x31); addbyte(0xc0); // XOR %eax,%eax
			} else {
				gen_load_reg(MULRM, EAX);
				addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
				addbyte(0x03); addbyte(0x46); addbyte(MULRN<<2); // ADD Rn,%eax
				gen_save_reg(MULRD, EAX);
			}
			break;
		}
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x33); addbyte(0x46); addbyte(RN<<2); // XOR Rn,%eax
		gen_save_reg(RD, EAX);
		break;

	case 0x03: /* EORS reg */
		if ((opcode & 0xf0) == 0x90) {
			/* MLAS */
			if (!flagsdirty) {
				addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV pcpsr,%cl
			}
			addbyte(0x80); addbyte(0xe1); addbyte(0x3f); // AND $~(NFLAG|ZFLAG),%cl
			if (MULRD == MULRM) {
				addbyte(0x31); addbyte(0xc0); // XOR %eax,%eax
			} else {
				gen_load_reg(MULRM, EAX);
				addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
				addbyte(0x03); addbyte(0x46); addbyte(MULRN<<2); // ADD Rn,%eax
				gen_save_reg(MULRD, EAX);
			}
			addbyte(0x85); addbyte(0xc0); // TEST %eax,%eax
			generatesetzn(opcode, pcpsr);
			break;
		}
		if (RD==15 || RN==15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) return 0;
		/* Shifted val now in %eax */
		flagsdirty = 0;
		addbyte(0x33); addbyte(0x46); addbyte(RN<<2); // XOR Rn,%eax
		gen_save_reg(RD, EAX);
		generatesetzn(opcode, pcpsr);
		break;

	case 0x04: /* SUB reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		gen_load_reg(RN, EDX);
		addbyte(0x29); addbyte(0xc2); // SUB %eax,%edx
		gen_save_reg(RD, EDX);
		break;

	case 0x05: /* SUBS reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV pcpsr,%cl
		addbyte(0x80); addbyte(0xe1); addbyte(0x0f); // AND $~(NFLAG|ZFLAG|CFLAG|VFLAG),%cl
		gen_load_reg(RN, EDX);
		addbyte(0x29); addbyte(0xc2); // SUB %eax,%edx
		gen_x86_lahf();
		gen_save_reg(RD, EDX);
		addbyte(0x0f); addbyte(0xb6); addbyte(0xd4); // MOVZBL %ah,%edx
		addbyte(0x71); addbyte(3); // JNO notoverflow
		addbyte(0x80); addbyte(0xc9); addbyte(0x10); // OR $VFLAG,%cl
		// .notoverflow
		addbyte(0x0a); addbyte(0x8a); addptr(lahftablesub); // OR lahftablesub(%edx),%cl
		addbyte(0x88); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV %cl,pcpsr
		break;

	case 0x06: /* RSB reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x2b); addbyte(0x46); addbyte(RN<<2); // SUB Rn,%eax
		gen_save_reg(RD, EAX);
		break;

	case 0x08: /* ADD reg */
		if ((opcode & 0xf0) == 0x90) {
			/* UMULL */
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
			gen_save_reg(MULRN, EAX);
			gen_save_reg(MULRD, EDX);
			break;
		}
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x03); addbyte(0x46); addbyte(RN<<2); // ADD Rn,%eax
		gen_save_reg(RD, EAX);
		break;

	case 0x09: /* ADDS reg */
		if ((opcode & 0xf0) == 0x90) {
			/* UMULLS */
			if (!flagsdirty) {
				addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV pcpsr,%cl
			}
			addbyte(0x80); addbyte(0xe1); addbyte(0x3f); // AND $~(NFLAG|ZFLAG),%cl
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x66); addbyte(MULRS<<2); // MULL Rs
			gen_save_reg(MULRN, EAX);
			gen_save_reg(MULRD, EDX);
			addbyte(0x85); addbyte(0xd2); // TEST %edx,%edx
			addbyte(0x79); addbyte(3); // JNS notn
			addbyte(0x80); addbyte(0xc9); addbyte(0x80); // OR $NFLAG,%cl
			addbyte(0x09); addbyte(0xd0); // OR %edx,%eax
			addbyte(0x75); addbyte(3); // JNZ testn
			addbyte(0x80); addbyte(0xc9); addbyte(0x40); // OR $ZFLAG,%cl
			addbyte(0x88); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV %cl,pcpsr
			flagsdirty = 1;
			break;
		}
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		flagsdirty = 0;
		addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV pcpsr,%cl
		addbyte(0x80); addbyte(0xe1); addbyte(0x0f); // AND $~(NFLAG|ZFLAG|CFLAG|VFLAG),%cl
		gen_load_reg(RN, EDX);
		addbyte(0x01); addbyte(0xc2); // ADD %eax,%edx
		gen_x86_lahf();
		gen_save_reg(RD, EDX);
		addbyte(0x71); addbyte(3); // JNO notoverflow
		addbyte(0x80); addbyte(0xc9); addbyte(0x10); // OR $VFLAG,%cl
		// .notoverflow
		addbyte(0xf6); addbyte(0xc4); addbyte(1); // TEST $1,%ah
		addbyte(0x74); addbyte(3); // JZ notc
		addbyte(0x80); addbyte(0xc9); addbyte(0x20); // OR $CFLAG,%cl
		// .notc
		/* Convenient trick here - Z & V flags are in the same place on x86 and ARM */
		addbyte(0x80); addbyte(0xe4); addbyte(0xc0); // AND $(NFLAG|ZFLAG),%ah
		addbyte(0x08); addbyte(0xe1); // OR %ah,%cl
		addbyte(0x88); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV %cl,pcpsr
		break;

	case 0x0a: /* ADC reg */
		flagsdirty = 0;
		if ((opcode & 0xf0) == 0x90) return 0; /* UMLAL */
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0xf6); addbyte(0x05); addptr(((char *) pcpsr) + 3); addbyte(0x20); // TESTB $0x20,(pcpsr>>24)
		addbyte(0x89); addbyte(0xc2); // MOV %eax,%edx
		gen_load_reg(RN, EAX);
		addbyte(0x74); addbyte(1); // JZ +1
		addbyte(0x42); // INC %edx
		addbyte(0x01); addbyte(0xd0); // ADD %edx,%eax
		gen_save_reg(RD, EAX);
		break;

	case 0x0b: /* ADCS reg */
		flagsdirty = 0;
		if ((opcode & 0xf0) == 0x90) return 0; /* UMLALS */
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV pcpsr,%cl
		addbyte(0x88); addbyte(0xcd); // MOV %cl,%ch
		addbyte(0x80); addbyte(0xe1); addbyte(0x0f); // AND $~(NFLAG|ZFLAG|CFLAG|VFLAG),%cl
		gen_load_reg(RN, EDX);
		addbyte(0xc0); addbyte(0xe5); addbyte(3); // SHL $3,%ch - put ARM carry into x86 carry
		addbyte(0x11); addbyte(0xc2); // ADC %eax,%edx
		gen_x86_lahf();
		gen_save_reg(RD, EDX);
		addbyte(0x0f); addbyte(0xb6); addbyte(0xd4); // MOVZBL %ah,%edx
		addbyte(0x71); addbyte(3); // JNO notoverflow
		addbyte(0x80); addbyte(0xc9); addbyte(0x10); // OR $VFLAG,%cl
		// .notoverflow
		addbyte(0x0a); addbyte(0x8a); addptr(lahftable); // OR lahftable(%edx),%cl
		addbyte(0x88); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV %cl,pcpsr
		break;

	case 0x0c: /* SBC reg */
		if ((opcode & 0xf0) == 0x90) {
			/* SMULL */
			gen_load_reg(MULRM, EAX);
			addbyte(0xf7); addbyte(0x6e); addbyte(MULRS<<2); // IMULL Rs
			gen_save_reg(MULRN, EAX);
			gen_save_reg(MULRD, EDX);
			break;
		}
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		flagsdirty = 0;
		addbyte(0xf6); addbyte(0x05); addptr(((char *) pcpsr) + 3); addbyte(0x20); // TESTB $0x20,(pcpsr>>24)
		addbyte(0x89); addbyte(0xc2); // MOV %eax,%edx
		gen_load_reg(RN, EAX);
		addbyte(0x75); addbyte(1); // JNZ +1
		addbyte(0x42); // INC %edx
		addbyte(0x29); addbyte(0xd0); // SUB %edx,%eax
		gen_save_reg(RD, EAX);
		break;

	case 0x0e: /* RSC reg */
		flagsdirty = 0;
		if ((opcode & 0xf0) == 0x90) {
			/* SMLAL */
			gen_load_reg(MULRM, EAX);
			gen_load_reg(MULRN, EBX);
			gen_load_reg(MULRD, ECX);
			addbyte(0xf7); addbyte(0x6e); addbyte(MULRS<<2); // IMULL Rs
			addbyte(0x01); addbyte(0xd8); // ADD %ebx,%eax
			addbyte(0x11); addbyte(0xca); // ADC %ecx,%edx
			gen_save_reg(MULRN, EAX);
			gen_save_reg(MULRD, EDX);
			break;
		}
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x31); addbyte(0xc9); // XOR %ecx,%ecx
		addbyte(0xf6); addbyte(0x05); addptr(((char *) pcpsr) + 3); addbyte(0x20); // TESTB $0x20,(pcpsr>>24)
		addbyte(0x89); addbyte(0xc2); // MOV %eax,%edx
		addbyte(0x0f); addbyte(0x94); addbyte(0xc1); // SETZ %cl
		gen_load_reg(RN, EAX);
		addbyte(0x29); addbyte(0xca); // SUB %ecx,%edx
		addbyte(0x29); addbyte(0xc2); // SUB %eax,%edx
		gen_save_reg(RD, EDX);
		break;

	case 0x11: /* TST reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x85); addbyte(0x46); addbyte(RN<<2); // TEST %eax,Rn
		generatesetzn(opcode, pcpsr);
		break;

	case 0x13: /* TEQ reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x33); addbyte(0x46); addbyte(RN<<2); // XOR Rn,%eax
		generatesetzn(opcode, pcpsr);
		break;

	case 0x15: /* CMP reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x8a); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV pcpsr,%cl
		addbyte(0x80); addbyte(0xe1); addbyte(0x0f); // AND $~(NFLAG|ZFLAG|CFLAG|VFLAG),%cl
		gen_load_reg(RN, EDX);
		addbyte(0x29); addbyte(0xc2); // SUB %eax,%edx
		gen_x86_lahf();
		addbyte(0x0f); addbyte(0xb6); addbyte(0xd4); // MOVZBL %ah,%edx
		addbyte(0x71); addbyte(3); // JNO notoverflow
		addbyte(0x80); addbyte(0xc9); addbyte(0x10); // OR $VFLAG,%cl
		// .notoverflow
		addbyte(0x0a); addbyte(0x8a); addptr(lahftablesub); // OR lahftablesub(%edx),%cl
		addbyte(0x88); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV %cl,pcpsr
		flagsdirty = 1;
		break;

	case 0x18: /* ORR reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x0b); addbyte(0x46); addbyte(RN<<2); // OR Rn,%eax
		gen_save_reg(RD, EAX);
		break;

	case 0x19: /* ORRS reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x0b); addbyte(0x46); addbyte(RN<<2); // OR Rn,%eax
		gen_save_reg(RD, EAX);
		generatesetzn(opcode, pcpsr);
		break;

	case 0x1a: /* MOV reg */
		flagsdirty = 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		if (RD == 15) {
			gen_load_reg(15, EDX);
			addbyte(0x83); addbyte(0xc0); addbyte(4); // ADD $4,%eax
			addbyte(0x81); addbyte(0xe2); addlong(~arm.r15_mask); // AND $(~arm.r15_mask),%edx
			addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
			addbyte(0x09); addbyte(0xd0); // OR %edx,%eax
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x1b: /* MOVS reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) return 0;
		/* Shifted val now in %eax */
		addbyte(0x85); addbyte(0xc0); // TEST %eax,%eax
		gen_save_reg(RD, EAX);
		generatesetzn2(opcode, pcpsr);
		break;

	case 0x1c: /* BIC reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0xf7); addbyte(0xd0); // NOT %eax
		addbyte(0x23); addbyte(0x46); addbyte(RN<<2); // AND Rn,%eax
		gen_save_reg(RD, EAX);
		break;

	case 0x1d: /* BICS reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) return 0;
		/* Shifted val now in %eax */
		addbyte(0xf7); addbyte(0xd0); // NOT %eax
		addbyte(0x23); addbyte(0x46); addbyte(RN<<2); // AND Rn,%eax
		gen_save_reg(RD, EAX);
		generatesetzn(opcode, pcpsr);
		break;

	case 0x1e: /* MVN reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generate_shift(opcode)) return 0;
		/* Shifted val now in %eax */
		addbyte(0xf7); addbyte(0xd0); // NOT %eax
		gen_save_reg(RD, EAX);
		break;

	case 0x1f: /* MVNS reg */
		flagsdirty = 0;
		if (RD==15 || RN==15) return 0;
		if (!generateshiftflags(opcode, pcpsr)) return 0;
		/* Shifted val now in %eax */
		addbyte(0xf7); addbyte(0xd0); // NOT %eax
		addbyte(0x85); addbyte(0xc0); // TEST %eax,%eax
		gen_save_reg(RD, EAX);
		generatesetzn(opcode, pcpsr);
		break;

	case 0x20: /* AND imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = arm_imm(opcode);
		generatedataproc(opcode, X86_OP_AND, templ);
		break;

	case 0x21: /* ANDS imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = generaterotate(opcode, pcpsr, 0xc0);
		// addbyte(0x80); addbyte(0xe1); addbyte(0x3f); // AND $~(NFLAG|ZFLAG),%cl
		generatedataprocS(opcode, X86_OP_AND, templ);
		generatesetznS(opcode, pcpsr);
		break;

	case 0x22: /* EOR imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = arm_imm(opcode);
		generatedataproc(opcode, X86_OP_XOR, templ);
		break;

	case 0x23: /* EORS imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = generaterotate(opcode, pcpsr, 0xc0);
		// addbyte(0x80); addbyte(0xe1); addbyte(0x3f); // AND $~(NFLAG|ZFLAG),%cl
		generatedataprocS(opcode, X86_OP_XOR, templ);
		generatesetznS(opcode, pcpsr);
		break;

	case 0x24: /* SUB imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = arm_imm(opcode);
		generatedataproc(opcode, X86_OP_SUB, templ);
		break;

	case 0x25: /* SUBS imm */
		flagsdirty = 0;
		if (RD==15) return 0;
		addbyte(0x80); addbyte(0x25); addptr(((char *) pcpsr) + 3); addbyte(0xf); // ANDB $0xf,pcpsr
		templ = arm_imm(opcode);
		generatedataprocS(opcode, X86_OP_SUB, templ);
		//gen_x86_lahf();
		addbyte(0x0f); addbyte(0x90); addbyte(0xc1); // SETO %cl
		addbyte(0x0f); addbyte(0xb6); addbyte(0xd4); // MOVZBL %ah,%edx
		addbyte(0xc0); addbyte(0xe1); addbyte(4); // SHL $4,%cl
		addbyte(0x0a); addbyte(0x8a); addptr(lahftablesub); // OR lahftablesub(%edx),%cl
		addbyte(0x08); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // OR %cl,pcpsr
		// flagsdirty = 1;
		break;

	case 0x28: /* ADD imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = arm_imm(opcode);
		generatedataproc(opcode, X86_OP_ADD, templ);
		break;

	case 0x29: /* ADDS imm */
		flagsdirty = 0;
		if (RD==15) return 0;
		addbyte(0x80); addbyte(0x25); addptr(((char *) pcpsr) + 3); addbyte(0xf); // ANDB $0xf,pcpsr
		templ = arm_imm(opcode);
		generatedataprocS(opcode, X86_OP_ADD, templ);
		addbyte(0x0f); addbyte(0x90); addbyte(0xc1); // SETO %cl
		addbyte(0x0f); addbyte(0xb6); addbyte(0xd4); // MOVZBL %ah,%edx
		addbyte(0xc0); addbyte(0xe1); addbyte(4); // SHL $4,%cl
		addbyte(0x0a); addbyte(0x8a); addptr(lahftable); // OR lahftable(%edx),%cl
		addbyte(0x08); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // OR %cl,pcpsr
		// flagsdirty = 1;
		break;

	case 0x31: /* TST imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = generaterotate(opcode, pcpsr, 0xc0);
		gen_load_reg(RN, EAX);
		addbyte(0x80); addbyte(0xe1); addbyte(0x3f); // AND $~(NFLAG|ZFLAG),%cl
		if (RN == 15 && arm.r15_mask != 0xfffffffc) {
			addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
		}
		addbyte(0xa9); addlong(templ); // TEST $templ,%eax
		generatesetzn(opcode, pcpsr);
		break;

	case 0x33: /* TEQ imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = generaterotate(opcode, pcpsr, 0xc0);
		gen_load_reg(RN, EAX);
		addbyte(0x80); addbyte(0xe1); addbyte(0x3f); // AND $~(NFLAG|ZFLAG),%cl
		if (RN == 15 && arm.r15_mask != 0xfffffffc) {
			addbyte(0x25); addlong(arm.r15_mask); // AND $arm.r15_mask,%eax
		}
		addbyte(0x35); addlong(templ); // XOR $templ,%eax
		generatesetzn(opcode, pcpsr);
		break;

	case 0x35: /* CMP imm */
		flagsdirty = 0;
		if (RD==15) return 0;
		addbyte(0x80); addbyte(0x25); addptr(((char *) pcpsr) + 3); addbyte(0xf); // ANDB $0xf,pcpsr
		templ = arm_imm(opcode);
		gen_load_reg(RN, EAX);
		addbyte(0x3d); addlong(templ); // CMP $templ,%eax
		gen_x86_lahf();
		addbyte(0x0f); addbyte(0x90); addbyte(0xc1); // SETO %cl
		addbyte(0x0f); addbyte(0xb6); addbyte(0xd4); // MOVZBL %ah,%edx
		addbyte(0xc0); addbyte(0xe1); addbyte(4); // SHL $4,%cl
		addbyte(0x0a); addbyte(0x8a); addptr(lahftablesub); // OR lahftablesub(%edx),%cl
		addbyte(0x08); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // OR %cl,pcpsr
		break;

	case 0x38: /* ORR imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = arm_imm(opcode);
		generatedataproc(opcode, X86_OP_OR, templ);
		break;

	case 0x39: /* ORRS imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = generaterotate(opcode, pcpsr, 0xc0);
		generatedataprocS(opcode, X86_OP_OR, templ);
		generatesetznS(opcode, pcpsr);
		break;

	case 0x3a: /* MOV imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = arm_imm(opcode);
		addbyte(0xc7); addbyte(0x46); addbyte(RD<<2); addlong(templ); // MOVL $templ,Rd
		break;

	case 0x3b: /* MOVS imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = generaterotate(opcode, pcpsr, 0xc0);
		// addbyte(0x80); addbyte(0xe1); addbyte(0x3f); // AND $~(NFLAG|ZFLAG),%cl
		if (templ == 0) {
			addbyte(0x80); addbyte(0xc9); addbyte(0x40); // OR $ZFLAG,%cl
		} else if (templ & 0x80000000) {
			addbyte(0x80); addbyte(0xc9); addbyte(0x80); // OR $NFLAG,%cl
		}
		addbyte(0xc7); addbyte(0x46); addbyte(RD<<2); addlong(templ); // MOVL $templ,Rd
		addbyte(0x88); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV %cl,pcpsr
		if ((opcode >> 28) == 0xe) {
			flagsdirty = 1;
		}
		break;

	case 0x3c: /* BIC imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = ~arm_imm(opcode);
		generatedataproc(opcode, X86_OP_AND, templ);
		break;

	case 0x3d: /* BICS imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = ~generaterotate(opcode, pcpsr, 0xc0);
		// addbyte(0x80); addbyte(0xe1); addbyte(0x3f); // AND $~(NFLAG|ZFLAG),%cl
		generatedataprocS(opcode, X86_OP_AND, templ);
		generatesetznS(opcode, pcpsr);
		break;

	case 0x3e: /* MVN imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = ~arm_imm(opcode);
		addbyte(0xc7); addbyte(0x46); addbyte(RD<<2); addlong(templ); // MOVL $templ,Rd
		break;

	case 0x3f: /* MVNS imm */
		// flagsdirty = 0;
		if (RD==15) return 0;
		templ = ~generaterotate(opcode, pcpsr, 0xc0);
		// addbyte(0x80); addbyte(0xe1); addbyte(0x3f); // AND $~(NFLAG|ZFLAG),%cl
		if (templ == 0) {
			addbyte(0x80); addbyte(0xc9); addbyte(0x40); // OR $ZFLAG,%cl
		} else if (templ & 0x80000000) {
			addbyte(0x80); addbyte(0xc9); addbyte(0x80); // OR $NFLAG,%cl
		}
		addbyte(0xc7); addbyte(0x46); addbyte(RD<<2); addlong(templ); // MOVL $templ,Rd
		addbyte(0x88); addbyte(0x0d); addptr(((char *) pcpsr) + 3); // MOV %cl,pcpsr
		if ((opcode >> 28) == 0xe) {
			flagsdirty = 1;
		}
		break;

	case 0x40: /* STR Rd, [Rn], #-imm   */
	case 0x48: /* STR Rd, [Rn], #+imm   */
	case 0x60: /* STR Rd, [Rn], -reg... */
	case 0x68: /* STR Rd, [Rn], +reg... */
		if (RD==15 || RN==15) return 0;
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode))
				return 0;
			gen_x86_mov_reg32_stack(EAX, 8);
		}
		flagsdirty = 0;
		gen_load_reg(RN, EBX);
		gen_load_reg(RD, ECX);
		genstr();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EAX, 8);
			if (opcode & 0x800000) {
				addbyte(0x01); addbyte(0x46); addbyte(RN<<2); /* ADD %eax,Rn */
			} else {
				addbyte(0x29); addbyte(0x46); addbyte(RN<<2); /* SUB %eax,Rn */
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x46); /* ADD */
				} else {
					addbyte(0x6e); /* SUB */
				}
				addbyte(RN<<2); addlong(offset);
			}
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		break;

	case 0x44: /* STRB Rd, [Rn], #-imm   */
	case 0x4c: /* STRB Rd, [Rn], #+imm   */
	case 0x64: /* STRB Rd, [Rn], -reg... */
	case 0x6c: /* STRB Rd, [Rn], +reg... */
		if (RD==15 || RN==15) return 0;
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode))
				return 0;
			gen_x86_mov_reg32_stack(EAX, 8);
		}
		flagsdirty = 0;
		gen_load_reg(RN, EBX);
		gen_load_reg(RD, ECX);
		genstrb();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EAX, 8);
			if (opcode & 0x800000) {
				addbyte(0x01); addbyte(0x46); addbyte(RN<<2); /* ADD %eax,Rn */
			} else {
				addbyte(0x29); addbyte(0x46); addbyte(RN<<2); /* SUB %eax,Rn */
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x46); /* ADD */
				} else {
					addbyte(0x6e); /* SUB */
				}
				addbyte(RN<<2); addlong(offset);
			}
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		break;

	case 0x41: /* LDR Rd, [Rn], #-imm   */
	case 0x49: /* LDR Rd, [Rn], #+imm   */
	case 0x61: /* LDR Rd, [Rn], -reg... */
	case 0x69: /* LDR Rd, [Rn], +reg... */
		if (RD==15 || RN==15) return 0;
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode))
				return 0;
			gen_x86_mov_reg32_stack(EAX, 8);
		}
		flagsdirty = 0;
		gen_load_reg(RN, EBX);
		genldr();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EDX, 8);
			if (opcode & 0x800000) {
				addbyte(0x01); addbyte(0x56); addbyte(RN<<2); /* ADD %edx,Rn */
			} else {
				addbyte(0x29); addbyte(0x56); addbyte(RN<<2); /* SUB %edx,Rn */
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x46); /* ADD */
				} else {
					addbyte(0x6e); /* SUB */
				}
				addbyte(RN<<2); addlong(offset);
			}
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x45: /* LDRB Rd, [Rn], #-imm   */
	case 0x4d: /* LDRB Rd, [Rn], #+imm   */
	case 0x65: /* LDRB Rd, [Rn], -reg... */
	case 0x6d: /* LDRB Rd, [Rn], +reg... */
		if (RD==15 || RN==15) return 0;
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode))
				return 0;
			gen_x86_mov_reg32_stack(EAX, 8);
		}
		flagsdirty = 0;
		gen_load_reg(RN, EBX);
		genldrb();
		if (opcode & 0x2000000) {
			gen_x86_mov_stack_reg32(EDX, 8);
			if (opcode & 0x800000) {
				addbyte(0x01); addbyte(0x56); addbyte(RN<<2); /* ADD %edx,Rn */
			} else {
				addbyte(0x29); addbyte(0x56); addbyte(RN<<2); /* SUB %edx,Rn */
			}
		} else {
			offset = opcode & 0xfff;
			if (offset != 0) {
				addbyte(0x81); // ADDL/SUBL $offset,Rn
				if (opcode & 0x800000) {
					addbyte(0x46); /* ADD */
				} else {
					addbyte(0x6e); /* SUB */
				}
				addbyte(RN<<2); addlong(offset);
			}
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x50: /* STR Rd, [Rn, #-imm]    */
	case 0x52: /* STR Rd, [Rn, #-imm]!   */
	case 0x58: /* STR Rd, [Rn, #+imm]    */
	case 0x5a: /* STR Rd, [Rn, #+imm]!   */
	case 0x70: /* STR Rd, [Rn, -reg...]  */
	case 0x72: /* STR Rd, [Rn, -reg...]! */
	case 0x78: /* STR Rd, [Rn, +reg...]  */
	case 0x7a: /* STR Rd, [Rn, +reg...]! */
		if (RD==15) return 0;
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode))
				return 0;
		} else {
			addbyte(0xb8); addlong(opcode & 0xfff); /* MOV $(opcode & 0xfff),%eax */
		}
		flagsdirty = 0;
		gen_load_reg(RN, EBX);
		if (RN == 15) {
			addbyte(0x81); addbyte(0xe3); addlong(arm.r15_mask); // AND $arm.r15_mask,%ebx
		}
		if (opcode & 0x800000) {
			addbyte(0x01); addbyte(0xc3); /* ADD %eax,%ebx */
		} else {
			addbyte(0x29); addbyte(0xc3); /* SUB %eax,%ebx */
		}
		gen_load_reg(RD, ECX);
		genstr();
		if (opcode & 0x200000) {
			/* Writeback */
			gen_save_reg(RN, EBX);
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		break;

	case 0x54: /* STRB Rd, [Rn, #-imm]    */
	case 0x56: /* STRB Rd, [Rn, #-imm]!   */
	case 0x5c: /* STRB Rd, [Rn, #+imm]    */
	case 0x5e: /* STRB Rd, [Rn, #+imm]!   */
	case 0x74: /* STRB Rd, [Rn, -reg...]  */
	case 0x76: /* STRB Rd, [Rn, -reg...]! */
	case 0x7c: /* STRB Rd, [Rn, +reg...]  */
	case 0x7e: /* STRB Rd, [Rn, +reg...]! */
		if (RD==15) return 0;
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode))
				return 0;
		} else {
			addbyte(0xb8); addlong(opcode & 0xfff); /* MOV $(opcode & 0xfff),%eax */
		}
		flagsdirty = 0;
		gen_load_reg(RN, EBX);
		if (RN == 15) {
			addbyte(0x81); addbyte(0xe3); addlong(arm.r15_mask); // AND $arm.r15_mask,%ebx
		}
		if (opcode & 0x800000) {
			addbyte(0x01); addbyte(0xc3); /* ADD %eax,%ebx */
		} else {
			addbyte(0x29); addbyte(0xc3); /* SUB %eax,%ebx */
		}
		gen_load_reg(RD, ECX);
		genstrb();
		if (opcode & 0x200000) {
			/* Writeback */
			gen_save_reg(RN, EBX);
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		break;

	case 0x51: /* LDR Rd, [Rn, #-imm]    */
	case 0x53: /* LDR Rd, [Rn, #-imm]!   */
	case 0x59: /* LDR Rd, [Rn, #+imm]    */
	case 0x5b: /* LDR Rd, [Rn, #+imm]!   */
	case 0x71: /* LDR Rd, [Rn, -reg...]  */
	case 0x73: /* LDR Rd, [Rn, -reg...]! */
	case 0x79: /* LDR Rd, [Rn, +reg...]  */
	case 0x7b: /* LDR Rd, [Rn, +reg...]! */
		if (RD==15) return 0;
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode))
				return 0;
		} else {
			addbyte(0xb8); addlong(opcode & 0xfff); /* MOV $(opcode & 0xfff),%eax */
		}
		flagsdirty = 0;
		gen_load_reg(RN, EBX);
		if (RN == 15) {
			addbyte(0x81); addbyte(0xe3); addlong(arm.r15_mask); // AND $arm.r15_mask,%ebx
		}
		if (opcode & 0x800000) {
			addbyte(0x01); addbyte(0xc3); /* ADD %eax,%ebx */
		} else {
			addbyte(0x29); addbyte(0xc3); /* SUB %eax,%ebx */
		}
		genldr();
		if (opcode & 0x200000) {
			/* Writeback */
			gen_save_reg(RN, EBX);
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x55: /* LDRB Rd, [Rn, #-imm]    */
	case 0x57: /* LDRB Rd, [Rn, #-imm]!   */
	case 0x5d: /* LDRB Rd, [Rn, #+imm]    */
	case 0x5f: /* LDRB Rd, [Rn, #+imm]!   */
	case 0x75: /* LDRB Rd, [Rn, -reg...]  */
	case 0x77: /* LDRB Rd, [Rn, -reg...]! */
	case 0x7d: /* LDRB Rd, [Rn, +reg...]  */
	case 0x7f: /* LDRB Rd, [Rn, +reg...]! */
		if (RD==15) return 0;
		if (opcode & 0x2000000) {
			if (!generate_shift(opcode))
				return 0;
		} else {
			addbyte(0xb8); addlong(opcode & 0xfff); /* MOV $(opcode & 0xfff),%eax */
		}
		flagsdirty = 0;
		gen_load_reg(RN, EBX);
		if (RN == 15) {
			addbyte(0x81); addbyte(0xe3); addlong(arm.r15_mask); // AND $arm.r15_mask,%ebx
		}
		if (opcode & 0x800000) {
			addbyte(0x01); addbyte(0xc3); /* ADD %eax,%ebx */
		} else {
			addbyte(0x29); addbyte(0xc3); /* SUB %eax,%ebx */
		}
		genldrb();
		if (opcode & 0x200000) {
			/* Writeback */
			gen_save_reg(RN, EBX);
		}
		if (!arm.abort_base_restored) {
			gen_test_armirq();
		}
		gen_save_reg(RD, EAX);
		break;

	case 0x80: /* STMDA */
	case 0x82: /* STMDA ! */
	case 0x90: /* STMDB */
	case 0x92: /* STMDB ! */
		if (RN == 15) {
			return 0;
		}
		flagsdirty = 0;
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_decrement(opcode, offset);
		gen_arm_store_multiple(opcode, offset);
		break;

	case 0x84: /* STMDA ^ */
	case 0x86: /* STMDA ^! */
	case 0x94: /* STMDB ^ */
	case 0x96: /* STMDB ^! */
		if (RN == 15) {
			return 0;
		}
		flagsdirty = 0;
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_decrement(opcode, offset);
		gen_arm_store_multiple_s(opcode, offset);
		break;

	case 0x88: /* STMIA */
	case 0x8a: /* STMIA ! */
	case 0x98: /* STMIB */
	case 0x9a: /* STMIB ! */
		if (RN == 15) {
			return 0;
		}
		flagsdirty = 0;
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_increment(opcode, offset);
		gen_arm_store_multiple(opcode, offset);
		break;

	case 0x8c: /* STMIA ^ */
	case 0x8e: /* STMIA ^! */
	case 0x9c: /* STMIB ^ */
	case 0x9e: /* STMIB ^! */
		if (RN == 15) {
			return 0;
		}
		flagsdirty = 0;
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_increment(opcode, offset);
		gen_arm_store_multiple_s(opcode, offset);
		break;

	case 0x81: /* LDMDA */
	case 0x83: /* LDMDA ! */
	case 0x91: /* LDMDB */
	case 0x93: /* LDMDB ! */
		if (RN == 15) {
			return 0;
		}
		flagsdirty = 0;
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_decrement(opcode, offset);
		gen_arm_load_multiple(opcode, offset);
		break;

	case 0x85: /* LDMDA ^ */
	case 0x87: /* LDMDA ^! */
	case 0x95: /* LDMDB ^ */
	case 0x97: /* LDMDB ^! */
		if (RN == 15 || (opcode & 0x8000)) {
			return 0;
		}
		flagsdirty = 0;
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_decrement(opcode, offset);
		gen_arm_load_multiple_s(opcode, offset);
		break;

	case 0x89: /* LDMIA */
	case 0x8b: /* LDMIA ! */
	case 0x99: /* LDMIB */
	case 0x9b: /* LDMIB ! */
		if (RN == 15) {
			return 0;
		}
		flagsdirty = 0;
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_increment(opcode, offset);
		gen_arm_load_multiple(opcode, offset);
		break;

	case 0x8d: /* LDMIA ^ */
	case 0x8f: /* LDMIA ^! */
	case 0x9d: /* LDMIB ^ */
	case 0x9f: /* LDMIB ^! */
		if (RN == 15 || (opcode & 0x8000)) {
			return 0;
		}
		flagsdirty = 0;
		offset = arm_ldm_stm_offset(opcode);
		gen_arm_ldm_stm_increment(opcode, offset);
		gen_arm_load_multiple_s(opcode, offset);
		break;

	case 0xa0: case 0xa1: case 0xa2: case 0xa3: /* B */
	case 0xa4: case 0xa5: case 0xa6: case 0xa7:
	case 0xa8: case 0xa9: case 0xaa: case 0xab:
	case 0xac: case 0xad: case 0xae: case 0xaf:
		flagsdirty = 0;
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
			addbyte(0x78); addbyte(12); /*JS endit*/

			addbyte(0x83); /*ADD $4,arm.reg[15]*/
			addbyte(0x05);
			addptr(&arm.reg[15]);
			addbyte(4);
			gen_x86_jump(CC_ALWAYS, block_enter); /*JMP start*/
			/*.endit*/
		}
#endif
		if (!flaglookup[opcode >> 28][(*pcpsr) >> 28]) {
			gen_x86_jump(CC_ALWAYS, 8);
		}
		break;

	case 0xb0: case 0xb1: case 0xb2: case 0xb3: /* BL */
	case 0xb4: case 0xb5: case 0xb6: case 0xb7:
	case 0xb8: case 0xb9: case 0xba: case 0xbb:
	case 0xbc: case 0xbd: case 0xbe: case 0xbf:
		flagsdirty = 0;
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
	if ((opcode >> 28) != 0xf) {
		flagsdirty = 0;
	}
	return 1;
}

void
generatecall(OpFn addr, uint32_t opcode, uint32_t *pcpsr)
{
        int old=codeblockpos;

        lastrecompiled=0;
        tempinscount++;
        if (recompileinstructions[(opcode>>20)&0xFF])
        {
                if (recompile(opcode,pcpsr)) return;
        }
        flagsdirty=0;
        codeblockpos=old;
        addbyte(0xC7); /*MOVL $opcode,(%esp)*/
        addbyte(0x04);
        addbyte(0x24);
        addlong(opcode);
        gen_x86_call(addr);
//#if 0
        if (!flaglookup[opcode>>28][(*pcpsr)>>28] && (opcode&0xE000000)==0xA000000)
        {
//                rpclog("Carrying on - %i\n",pcinc);
//                generateupdatepc();
                if (pcinc)
                {
                        addbyte(0x83); /* ADD $pcinc,arm.reg[15] */
                        addbyte(0x05);
                        addptr(&arm.reg[15]);
                        addbyte(pcinc);
//                pcinc=0;
                }
                gen_x86_jump(CC_ALWAYS, 8);
        }
	if (lastflagchange != 0) {
		gen_x86_jump_here_long(lastflagchange);
	}
//        #endif
}

void
generateupdatepc(void)
{
	if (pcinc != 0) {
		addbyte(0x83); addbyte(0x46); addbyte(15<<2); addbyte(pcinc); /* ADD $pcinc,R15 */
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
			addbyte(0x83); addbyte(0x05); addptr(&inscount); addbyte(tempinscount); // ADDL $tempinscount,inscount
		}
		tempinscount = 0;
	}
}

void
generatepcinc(void)
{
        pcinc+=4;
	if (pcinc == 124) {
		generateupdatepc();
	}
        if (codeblockpos>=1800) blockend=1;
}

void
endblock(uint32_t opcode)
{
	flagsdirty = 0;

	generateupdatepc();
	generateupdateinscount();

	gen_x86_jump_here_long(9);

	addbyte(0xff); addbyte(0x0d); addptr(&linecyc); // DECL linecyc
	gen_x86_jump(CC_S, 0);

	addbyte(0xf6); addbyte(0x05); addptr(&armirq); addbyte(0xff); // TESTB $0xff,armirq
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

	/* Jump to next block bypassing function prologue */
	addbyte(0x83); addbyte(0xc0); addbyte(block_enter); // ADD $block_enter,%eax
	addbyte(0xff); addbyte(0xe0); // JMP *%eax
}

void
generateflagtestandbranch(uint32_t opcode, uint32_t *pcpsr)
{
	int cond;

	if ((opcode >> 28) == 0xe) {
		/* No need if 'always' condition code */
		return;
	}
	switch (opcode >> 28) {
	case 0: /* EQ */
	case 1: /* NE */
		if (flagsdirty) {
			addbyte(0xf6); addbyte(0xc1); addbyte(0x40); // TEST $0x40,%cl
		} else {
			addbyte(0xF6); /*TESTB (pcpsr>>24),$0x40*/
			addbyte(0x05);
			addptr(((char *) pcpsr) + 3);
			addbyte(0x40);
		}
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	case 2: /* CS */
	case 3: /* CC */
		if (flagsdirty) {
			addbyte(0xf6); addbyte(0xc1); addbyte(0x20); // TEST $0x20,%cl
		} else {
			addbyte(0xF6); /*TESTB (pcpsr>>24),$0x20*/
			addbyte(0x05);
			addptr(((char *) pcpsr) + 3);
			addbyte(0x20);
		}
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	case 4: /* MI */
	case 5: /* PL */
		if (flagsdirty) {
			addbyte(0xf6); addbyte(0xc1); addbyte(0x80); // TEST $0x80,%cl
		} else {
			addbyte(0xF6); /*TESTB (pcpsr>>24),$0x80*/
			addbyte(0x05);
			addptr(((char *) pcpsr) + 3);
			addbyte(0x80);
		}
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	case 6: /* VS */
	case 7: /* VC */
		if (flagsdirty) {
			addbyte(0xf6); addbyte(0xc1); addbyte(0x10); // TEST $0x10,%cl
		} else {
			addbyte(0xF6); /*TESTB (pcpsr>>24),$0x10*/
			addbyte(0x05);
			addptr(((char *) pcpsr) + 3);
			addbyte(0x10);
		}
		cond = ((opcode >> 28) & 1) ? CC_NE : CC_E;
		break;
	default:
		if (flagsdirty) {
			addbyte(0x0f); addbyte(0xb6); addbyte(0xc1); // MOVZBL %cl,%eax
			addbyte(0xc1); addbyte(0xe8); addbyte(4); // SHR $4,%eax
		} else {
			addbyte(0xa1); addptr(pcpsr); // MOV pcpsr,%eax
			addbyte(0xc1); addbyte(0xe8); addbyte(28); // SHR $28,%eax
		}
		addbyte(0x80); addbyte(0xb8); addptr(&flaglookup[opcode >> 28][0]); addbyte(0); // CMPB $0,flaglookup(%eax)
		cond = CC_E;
		break;
	}
	// flagsdirty = 0;
	lastflagchange = gen_x86_jump_forward_long(cond);
}

void
generateirqtest(void)
{
	if (lastrecompiled) {
		return;
	}

        addbyte(0x85); /*TESTL %eax,%eax*/
        addbyte(0xC0);
	gen_x86_jump(CC_NE, 0);
	if (lastflagchange != 0) {
		gen_x86_jump_here_long(lastflagchange);
	}
}

#endif

