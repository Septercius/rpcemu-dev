/*r15 is pointer to armregs
  r14 is vwaddrl
  r13 is vraddrl
  r12 contains R15*/

#include "rpcemu.h"
#ifdef DYNAREC
#ifdef __amd64__

#include <stdint.h>
#include "codegen_amd64.h"
#include "mem.h"
#include "arm.h"

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#endif

int hasldrb[BLOCKS];
void generateupdatepc(void);
int lastflagchange;
unsigned char rcodeblock[BLOCKS][1792];
static uint64_t codeblockaddr[BLOCKS];
uint32_t codeblockpc[0x8000];
int codeblocknum[0x8000];
static unsigned char codeblockpresent[0x10000];

//#define BLOCKS 4096
//#define HASH(l) ((l>>3)&0x3FFF)
int blockend;
static int blocknum;//,blockcount;
static int tempinscount;

static int codeblockpos;
static int lastjumppos;

#define addbyte(a)         rcodeblock[blockpoint2][codeblockpos]=(unsigned char) (a),codeblockpos++
#define addlong(a)         *((unsigned long *)&rcodeblock[blockpoint2][codeblockpos])=(unsigned long) (a),codeblockpos+=4

static int blockpoint = 0, blockpoint2;
static uint32_t blocks[BLOCKS];
static int pcinc = 0;
static int lastrecompiled = 0;

#include "codegen_x86_common.h"

void initcodeblocks(void)
{
        int c;
#ifdef __linux__
	void *start;
	size_t len;
	long pagesize = sysconf(_SC_PAGESIZE);
	long pagemask = ~(pagesize - 1);
#endif
        /*Clear all blocks*/
        memset(codeblockpc,0xFF,4*0x1000);
//        memset(codeblockcount,0,0x1000);
        blockpoint=0;
        for (c=0;c<BLOCKS;c++) blocks[c]=0xFFFFFFFF;
        for (c=0;c<BLOCKS;c++) codeblockaddr[c]=(uint64_t) &rcodeblock[c][BLOCKSTART];

#ifdef __linux__
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

void resetcodeblocks(void)
{
        int c;
        /*Clear all blocks _except_ those pointing between 0x3800000 and 0x3FFFFFF (ROM)*/
/*        for (c=0;c<0x1000;c++)
        {
                if ((codeblockpc[c][2]&0xFF800000)!=0x3800000)
                   codeblockpc[c][2]=0xFFFFFFFF;
                if ((codeblockpc[c][1]&0xFF800000)!=0x3800000)
                {
                        codeblockpc[c][1]=0xFFFFFFFF;
                        codeblockcount[c]=1;
                }
                if ((codeblockpc[c][0]&0xFF800000)!=0x3800000)
                {
                        codeblockpc[c][0]=0xFFFFFFFF;
                        codeblockcount[c]=0;
                }
        }*/
        blockpoint=0;
        for (c=0;c<BLOCKS;c++)
        {
                if (blocks[c]!=0xFFFFFFFF)
                {
                        if ((codeblockpc[blocks[c]&0x7FFF]&0xFF800000)!=0x3800000)
                        {
                                codeblockpc[blocks[c]&0x7FFF]=0xFFFFFFFF;
                                codeblocknum[blocks[c]&0x7FFF]=0xFFFFFFFF;
                                blocks[c]=0xFFFFFFFF;
                        }
                }
        }
}

//#if 0
void cacheclearpage(uint32_t a)
{
        int c,d;
        if (!codeblockpresent[a&0xFFFF]) return;
        codeblockpresent[a&0xFFFF]=0;
        ins++;
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
//#endif

/*int isblockvalid(unsigned long l)
{
        if ((l&0xFFC00000)==0x3800000) return 1;
        return 0;
}*/

void initcodeblock(uint32_t l)
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
	//printf("New block %08X %08X %08X\n",blocknum,l,codeblockpc[blocknum]);
        codeblocknum[blocknum]=blockpoint;
        blocks[blockpoint]=blocknum;
        blockpoint2=blockpoint;
	addbyte(0x45); addbyte(0x89); addbyte(0x67); addbyte(15<<2); /*MOVL %r12d,R15*/
	addbyte(0x48); /*ADDL $8,%rsp*/
        addbyte(0x83);
        addbyte(0xC4);
        addbyte(0x08);
        addbyte(0xC3); /*RET*/
addbyte(0); addbyte(0); addbyte(0);
	addbyte(0x48); /*SUBL $8,%rsp*/
        addbyte(0x83);
        addbyte(0xEC);
        addbyte(0x08);
	addbyte(0x49); /*MOVQ armregs,%r15*/
	addbyte(0xBF);
	addlong(&armregs[0]);
	addlong(((uint64_t)(&armregs[0]))>>32);
	addbyte(0x49); /*MOVQ vwaddrl,%r14*/
	addbyte(0xBE);
	addlong(&vwaddrl[0]);
	addlong(((uint64_t)(&vwaddrl[0]))>>32);
	addbyte(0x49); /*MOVQ vwaddrl,%r13*/
	addbyte(0xBD);
	addlong(&vraddrl[0]);
	addlong(((uint64_t)(&vraddrl[0]))>>32);
	addbyte(0x45); addbyte(0x8B); addbyte(0x67); addbyte(15<<2); /*MOVL R15,%r12d*/
//	printf("New block %08X %08X %08X\n",blocknum,l,codeblockpc[blocknum]);
}

static int recompreadmemb(uint32_t addr)
{
	asm("push %r12;");
	uint32_t temp=readmemb(addr);
        asm("movl %0,%%edx;"
            :
            : "r" (temp)
        );
	asm("pop %r12;");
	return (armirq&0x40)?1:0;
}

static int recompreadmeml(uint32_t addr)
{
	asm("push %rdi; push %r12");
	uint32_t temp=readmeml(addr);
        asm("movl %0,%%edx;"
            :
            : "r" (temp)
        );
	asm("pop %r12; pop %rdi;");
	return (armirq&0x40)?1:0;
}

static int recompwritememb(uint32_t addr)
{
	asm("push %r12;");
	register uint8_t v asm("al");
	writememb(addr,v);
	asm("pop %r12;");
	return (armirq&0x40)?1:0;
}

static int recompwritememl(uint32_t addr)
{
	asm("push %rdi; push %r12;");
	register uint32_t v asm("eax");
	writememl(addr,v);
	asm("pop %r12; pop %rdi;");
	return (armirq&0x40)?1:0;
}

static const int canrecompile[256]=
{
	1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0, /*00*/
	0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0, /*10*/
	1,0,1,0,1,0,0,0,1,0,1,0,0,0,0,0, /*20*/
	0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0, /*30*/

	1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, /*40*/
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*50*/
	1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0, /*60*/
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*70*/

	1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /*80*/
	1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /*90*/
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*A0*/
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*B0*/

	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /*C0*/
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /*D0*/
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /*E0*/
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /*F0*/
};

static void genstoreimm(int reg, uint32_t val)
{
	if (reg==15) printf("genstoreimm R15!\n");
	addbyte(0x41); addbyte(0xC7); addbyte(0x47); /*MOVL $val,reg(r15)*/
	addbyte(reg<<2); addlong(val);
}

static void genloadreggen(int reg, int x86reg)
{
	if (reg==15) { addbyte(0x44); addbyte(0x89); addbyte(0xE0|(x86reg>>3)); /*MOVL %r12d,%eax*/ }
	else         { addbyte(0x41); addbyte(0x8B); addbyte(0x47|x86reg); addbyte(reg<<2); /*MOVL RN,%eax*/ }
}

static inline void genloadreg(int reg) /*Assumes %eax as targer*/
{
	genloadreggen(reg, EAX);
}

static void genstorereggen(int reg, int x86reg)
{
	if (reg==15) { addbyte(0x41); addbyte(0x89); addbyte(0xC4|x86reg); }
	else         { addbyte(0x41); addbyte(0x89); addbyte(0x47|x86reg); addbyte(reg<<2); /*MOVL %eax,RD*/ }
}

static inline void genstorereg(int reg) /*Assumes %eax as source*/
{
	genstorereggen(reg, EAX);
}

static void generatedataproc(uint32_t opcode, uint8_t op, uint32_t val)
{
	if (RN==RD) /*Can use RMW instruction*/
	{
		if (val&~0x7F)
		{
			addbyte(0x41); addbyte(0x81); addbyte(0x47|op); addbyte(RD<<2);
			addlong(val);
		}
		else
		{
			addbyte(0x41); addbyte(0x83); addbyte(0x47|op); addbyte(RD<<2);
			addbyte(val);
		}
	}
	else /*Load/modify/store*/
	{
		genloadreg(RN);
		if (RN==15)
		{
			addbyte(0x25); addlong(r15mask); /*AND $r15mask,%eax*/
		}
		addbyte(0x05|op); addlong(val); /*OP $val,%eax*/
		genstorereg(RD);
	}
}

static void generateregdataproc(uint32_t opcode, uint8_t op, int dirmatters)
{
	if (dirmatters || RN==15)
	{
		genloadreggen(RN,EDX);
		if (RN==15) { addbyte(0x81); addbyte(0xE2); addlong(r15mask); }
		addbyte(0x01|op); addbyte(0xC2); /*OP %eax,%edx*/
		genstorereggen(RD,EDX);
	}
	else
	{
		addbyte(0x41); addbyte(0x03|op); addbyte(0x47); addbyte(RN<<2); /*OP RN,%eax*/
		genstorereg(RD);
	}
}

static int generateshift(uint32_t opcode, uint32_t *pcpsr)
{
	unsigned int temp;
	if (opcode&0x10) return 0; /* Can't do register shifts or multiplies */
	if (!(opcode&0xFF0))
	{
		genloadreg(RM);
		return 1;
	}
	temp=(opcode>>7)&31;
        switch (opcode&0x60)
        {
                case 0x00: /*LSL*/
                genloadreg(RM);
                if (temp) addbyte(0xC1); addbyte(0xE0); addbyte(temp); /*SHL $temp,%eax*/
                return 1;
                case 0x20: /*LSR*/
                if (temp)
                {
                        genloadreg(RM);
                        addbyte(0xC1); addbyte(0xE8); addbyte(temp); /*SHR $temp,%eax*/
                }
                else
                {
                        addbyte(0x31); addbyte(0xC0); /*XOR %eax,%eax*/
                }
                return 1;
                case 0x40: /*ASR*/
                if (!temp) temp=31;
                genloadreg(RM);
                addbyte(0xC1); addbyte(0xF8); addbyte(temp); /*SAR $temp,%eax*/
                return 1;
                case 0x60: /*ROR*/
                if (!temp) break;
                genloadreg(RM);
                addbyte(0xC1); addbyte(0xC8); addbyte(temp); /*ROR $temp,%eax*/
                return 1;
        }
        return 0;

}

static void genldr(void) /*address in %edi, data in %eax*/
{
	addbyte(0x57); /*PUSH %rdi*/
	addbyte(0x89); addbyte(0xFA); /*MOV %edi,%edx*/
	addbyte(0xC1); addbyte(0xEA); addbyte(12); /*SHRL $12,%edx*/
	addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edi*/
	addbyte(0x49); addbyte(0x8B); addbyte(0x54); addbyte(0xD5); addbyte(0); /*MOVQ (%r13,%edx,8),%rdx*/
	addbyte(0xF6); addbyte(0xC2); addbyte(1); /*TESTB %dl,1*/
	addbyte(0x75); addbyte(7); /*JNZ slow*/
	addbyte(0x8B); addbyte(0x14); addbyte(0x3A); /*MOVL (%rdx,%rdi),%edx*/
	addbyte(0x30); addbyte(0xC0); /*XOR %al,%al*/
	addbyte(0xEB); addbyte(5); /*JMP over*/
	gen_x86_call(recompreadmeml);
	addbyte(0x5F); /*POP %rdi*/
	addbyte(0x89); addbyte(0xF9); /*MOVL %edi,%ecx*/
	addbyte(0xC1); addbyte(0xE1); addbyte(3); /*SHL $3,%ecx*/
	addbyte(0xD3); addbyte(0xCA); /*ROR %cl,%edx*/

}

static void genldrb(void) /*address in %edi, data in %al*/
{
	addbyte(0x89); addbyte(0xFA); /*MOV %edi,%edx*/
	addbyte(0xC1); addbyte(0xEA); addbyte(12); /*SHRL $12,%edx*/
	addbyte(0x89); addbyte(0xFF); /*MOVL %edi,%edi*/
	addbyte(0x49); addbyte(0x8B); addbyte(0x54); addbyte(0xD5); addbyte(0); /*MOVQ (%r13,%edx,8),%rdx*/
	addbyte(0xF6); addbyte(0xC2); addbyte(1); /*TESTB %dl,1*/
	addbyte(0x75); addbyte(8); /*JNZ slow*/
	addbyte(0x0F); addbyte(0xB6); addbyte(0x14); addbyte(0x3A); /*MOVZX (%rdx,%rdi),%edx*/
	addbyte(0x30); addbyte(0xC0); /*XOR %al,%al*/
	addbyte(0xEB); addbyte(7); /*JMP over*/
	addbyte(0x57); /*PUSH %rdi*/
	gen_x86_call(recompreadmemb);
	addbyte(0x5F); /*POP %rdi*/
}

//addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edi*/
static void genstr(void) /*address in %edi, data in %eax*/
{
	addbyte(0x57); /*PUSH %rdi*/
	addbyte(0x89); addbyte(0xFA); /*MOV %edi,%edx*/
	addbyte(0xC1); addbyte(0xEA); addbyte(12); /*SHRL $12,%edx*/
	addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edi*/
	//addbyte(0x89); addbyte(0xFF); /*MOVL %edi,%edi*/
	addbyte(0x49); addbyte(0x8B); addbyte(0x14); addbyte(0xD6); /*MOVQ (%r14,%edx,8),%rdx*/
	addbyte(0xF6); addbyte(0xC2); addbyte(3); /*TESTB %dl,3*/
	addbyte(0x75); addbyte(7); /*JNZ slow*/
	addbyte(0x89); addbyte(0x04); addbyte(0x3A); /*MOV %eax,(%rdx,%rdi)*/
	addbyte(0x30); addbyte(0xC0); /*XOR %al,%al*/
	addbyte(0xEB); addbyte(5); /*JMP over*/
	gen_x86_call(recompwritememl);
	addbyte(0x5F); /*POP %rdi*/
}

static void genstrb(void) /*address in %edi, data in %al*/
{
	addbyte(0x89); addbyte(0xFA); /*MOV %edi,%edx*/
	addbyte(0xC1); addbyte(0xEA); addbyte(12); /*SHRL $12,%edx*/
	addbyte(0x89); addbyte(0xFF); /*MOVL %edi,%edi*/
	addbyte(0x49); addbyte(0x8B); addbyte(0x14); addbyte(0xD6); /*MOVQ (%r14,%edx,8),%rdx*/
	addbyte(0xF6); addbyte(0xC2); addbyte(3); /*TESTB %dl,3*/
	addbyte(0x75); addbyte(7); /*JNZ slow*/
	addbyte(0x88); addbyte(0x04); addbyte(0x3A); /*MOVB %al,(%rdx,%rdi)*/
	addbyte(0x30); addbyte(0xC0); /*XOR %al,%al*/
	addbyte(0xEB); addbyte(7); /*JMP over*/
	addbyte(0x57); /*PUSH %rdi*/
	gen_x86_call(recompwritememb);
	addbyte(0x5F); /*POP %rdi*/
}

static void gentestabort(void)
{
	addbyte(0x84); /*TESTL %al,%al*/
	addbyte(0xC0);
	if (((char *)(&rcodeblock[blockpoint2][codeblockpos+4])-(char *)(&rcodeblock[blockpoint2][0]))<120)
	{
		addbyte(0x75); /*JNE 0*/
		addbyte((char *)&rcodeblock[blockpoint2][0]-(char *)(&rcodeblock[blockpoint2][codeblockpos+1]));
	}
	else
	{
		addbyte(0x0F); /*JNE 0*/
		addbyte(0x85);
		addrel32(&rcodeblock[blockpoint2][0]);
	}
}

static int recompile(uint32_t opcode, uint32_t *pcpsr)
{
	int c;
	uint32_t templ;

	switch ((opcode>>20)&0xFF)
	{
	case 0x00: /* AND reg */
		if (RD==15) return 0;
		if (!generateshift(opcode,pcpsr)) return 0;
		generateregdataproc(opcode, X86_OP_AND, 1);
		break;

	case 0x02: /* EOR reg */
		if (RD==15) return 0;
		if (!generateshift(opcode,pcpsr)) return 0;
		generateregdataproc(opcode, X86_OP_XOR, 0);
		break;

	case 0x04: /* SUB reg */
		if (RD==15) return 0;
		if (!generateshift(opcode,pcpsr)) return 0;
		generateregdataproc(opcode, X86_OP_SUB, 1);
		break;

	case 0x08: /* ADD reg */
		if (RD==15) return 0;
		if (!generateshift(opcode,pcpsr)) return 0;
		generateregdataproc(opcode, X86_OP_ADD, 0);
		break;

	case 0x0a: /* ADC reg */
                if (RD==15) return 0;
		if (!generateshift(opcode,pcpsr)) return 0;
		genloadreggen(15,ECX);
		addbyte(0xC1); addbyte(0xE1); addbyte(3); /*SHL $3,%ecx - puts ARM carry into x64 carry*/
                generateregdataproc(opcode, X86_OP_ADC, 0);
                break;

	case 0x18: /* ORR reg */
		if (RD==15) return 0;
		if (!generateshift(opcode,pcpsr)) return 0;
		generateregdataproc(opcode, X86_OP_OR, 0);
		break;

	case 0x1a: /* MOV reg */
		if (RD==15) return 0;
		if (!generateshift(opcode,pcpsr)) return 0;
		genstorereg(RD);
		break;

	case 0x20: /* AND imm */
                if (RD==15) return 0;
                templ=rotate2(opcode);
                generatedataproc(opcode, X86_OP_AND, templ);
                break;

	case 0x22: /* EOR imm */
                if (RD==15) return 0;
                templ=rotate2(opcode);
                generatedataproc(opcode, X86_OP_XOR, templ);
                break;

	case 0x24: /* SUB imm */
                if (RD==15) return 0;
                templ=rotate2(opcode);
                generatedataproc(opcode, X86_OP_SUB, templ);
                break;

	case 0x28: /* ADD imm */
                if (RD==15) return 0;
                templ=rotate2(opcode);
                generatedataproc(opcode, X86_OP_ADD, templ);
                break;

	case 0x2a: /* ADC imm */
                if (RD==15) return 0;
		genloadreggen(15,ECX);
		addbyte(0xC1); addbyte(0xE1); addbyte(3); /*SHL $3,%ecx - puts ARM carry into x64 carry*/
                templ=rotate2(opcode);
                generatedataproc(opcode, X86_OP_ADC, templ);
                break;

	case 0x38: /* ORR imm */
                if (RD==15) return 0;
                templ=rotate2(opcode);
                generatedataproc(opcode, X86_OP_OR, templ);
                break;

	case 0x3a: /* MOV imm */
                if (RD==15) return 0;
                templ=rotate2(opcode);
		genstoreimm(RD,templ);
		break;

	case 0x40: case 0x48: /* STR Rd, [Rn], #      */
	case 0x60: case 0x68: /* STR Rd, [Rn], reg... */
		if (RD==15 || RN==15) return 0;
		if (opcode&0x2000000) { if (!generateshift(opcode,pcpsr)) return 0;
					addbyte(0x50); /*PUSH %rax*/}
		templ=opcode&0xFFF;
		genloadreggen(RN,EDI);
		addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edi*/
		genloadreg(RD);
		genstr();
	        addbyte(0x84); /*TESTL %al,%al*/
	        addbyte(0xC0);
		if (opcode&0x2000000) { addbyte(0x58); /*POP %rax*/ }
		if (((char *)(&rcodeblock[blockpoint2][codeblockpos+4])-(char *)(&rcodeblock[blockpoint2][0]))<120)
        	{
                	addbyte(0x75); /*JNE 0*/
                	addbyte((char *)&rcodeblock[blockpoint2][0]-(char *)(&rcodeblock[blockpoint2][codeblockpos+1]));
        	}
        	else
        	{
                	addbyte(0x0F); /*JNE 0*/
                	addbyte(0x85);
                	addrel32(&rcodeblock[blockpoint2][0]);
        	}
		if (opcode&0x2000000)
		{
			if (opcode&0x800000) { addbyte(0x41); addbyte(0x01); addbyte(0x47); addbyte(RN<<2); /*ADDL %eax,Rn*/ }
			else		     { addbyte(0x41); addbyte(0x29); addbyte(0x47); addbyte(RN<<2); /*SUBL %eax,Rn*/ }
		}
		else
		{
			if (templ)
			{
				if (opcode&0x800000) { addbyte(0x41); addbyte(0x81); addbyte(0x47); addbyte(RN<<2); addlong(templ); /*ADDL $temp,Rn*/ }
				else		     { addbyte(0x41); addbyte(0x81); addbyte(0x6F); addbyte(RN<<2); addlong(templ); /*SUBL $temp,Rn*/ }
			}
		}
		break;

	case 0x44: case 0x4c: /* STRB Rd, [Rn], #      */
	case 0x64: case 0x6c: /* STRB Rd, [Rn], reg... */
		if (RD==15 || RN==15) return 0;
		if (opcode&0x2000000) { if (!generateshift(opcode,pcpsr)) return 0;
					addbyte(0x50); /*PUSH %rax*/}
		templ=opcode&0xFFF;
		genloadreggen(RN,EDI);
		genloadreg(RD);
		genstrb();
	        addbyte(0x84); /*TESTL %al,%al*/
	        addbyte(0xC0);
		if (opcode&0x2000000) { addbyte(0x58); /*POP %rax*/ }
		if (((char *)(&rcodeblock[blockpoint2][codeblockpos+4])-(char *)(&rcodeblock[blockpoint2][0]))<120)
        	{
                	addbyte(0x75); /*JNE 0*/
                	addbyte((char *)&rcodeblock[blockpoint2][0]-(char *)(&rcodeblock[blockpoint2][codeblockpos+1]));
        	}
        	else
        	{
                	addbyte(0x0F); /*JNE 0*/
                	addbyte(0x85);
                	addrel32(&rcodeblock[blockpoint2][0]);
        	}
		if (opcode&0x2000000)
		{
			if (opcode&0x800000) { addbyte(0x41); addbyte(0x01); addbyte(0x47); addbyte(RN<<2); /*ADDL %eax,Rn*/ }
			else		     { addbyte(0x41); addbyte(0x29); addbyte(0x47); addbyte(RN<<2); /*SUBL %eax,Rn*/ }
		}
		else
		{
			if (templ)
			{
				if (opcode&0x800000) { addbyte(0x41); addbyte(0x81); addbyte(0x47); addbyte(RN<<2); addlong(templ); /*ADDL $temp,Rn*/ }
				else		     { addbyte(0x41); addbyte(0x81); addbyte(0x6F); addbyte(RN<<2); addlong(templ); /*SUBL $temp,Rn*/ }
			}
		}
		break;

	case 0x41: case 0x49: /* LDR Rd, [Rn], #      */
	case 0x61: case 0x69: /* LDR Rd, [Rn], reg... */
		if (RD==15 || RN==15) return 0;
		if (opcode&0x2000000) { if (!generateshift(opcode,pcpsr)) return 0;
					addbyte(0x50); /*PUSH %rax*/}
		templ=opcode&0xFFF;
		genloadreggen(RN,EDI);
		genldr();
	        addbyte(0x84); /*TESTL %al,%al*/
	        addbyte(0xC0);
		if (opcode&0x2000000) { addbyte(0x58); /*POP %rax*/ }
		if (((char *)(&rcodeblock[blockpoint2][codeblockpos+4])-(char *)(&rcodeblock[blockpoint2][0]))<120)
        	{
                	addbyte(0x75); /*JNE 0*/
                	addbyte((char *)&rcodeblock[blockpoint2][0]-(char *)(&rcodeblock[blockpoint2][codeblockpos+1]));
        	}
        	else
        	{
                	addbyte(0x0F); /*JNE 0*/
                	addbyte(0x85);
                	addrel32(&rcodeblock[blockpoint2][0]);
        	}
		genstorereggen(RD,EDX);
		if (opcode&0x2000000)
		{
			if (opcode&0x800000) { addbyte(0x41); addbyte(0x01); addbyte(0x47); addbyte(RN<<2); /*ADDL %eax,Rn*/ }
			else		     { addbyte(0x41); addbyte(0x29); addbyte(0x47); addbyte(RN<<2); /*SUBL %eax,Rn*/ }
		}
		else
		{
			if (templ)
			{
				if (opcode&0x800000) { addbyte(0x41); addbyte(0x81); addbyte(0x47); addbyte(RN<<2); addlong(templ); /*ADDL $temp,Rn*/ }
				else		     { addbyte(0x41); addbyte(0x81); addbyte(0x6F); addbyte(RN<<2); addlong(templ); /*SUBL $temp,Rn*/ }
			}
		}
		break;

	case 0x45: case 0x4d: /* LDRB Rd, [Rn], #      */
	case 0x65: case 0x6d: /* LDRB Rd, [Rn], reg... */
		if (RD==15 || RN==15) return 0;
		if (opcode&0x2000000) { if (!generateshift(opcode,pcpsr)) return 0;
					addbyte(0x50); /*PUSH %rax*/}
		templ=opcode&0xFFF;
		genloadreggen(RN,EDI);
		genldrb();
	        addbyte(0x84); /*TESTL %al,%al*/
	        addbyte(0xC0);
		if (opcode&0x2000000) { addbyte(0x58); /*POP %rax*/ }
		if (((char *)(&rcodeblock[blockpoint2][codeblockpos+4])-(char *)(&rcodeblock[blockpoint2][0]))<120)
        	{
                	addbyte(0x75); /*JNE 0*/
                	addbyte((char *)&rcodeblock[blockpoint2][0]-(char *)(&rcodeblock[blockpoint2][codeblockpos+1]));
        	}
        	else
        	{
                	addbyte(0x0F); /*JNE 0*/
                	addbyte(0x85);
                	addrel32(&rcodeblock[blockpoint2][0]);
        	}
		genstorereggen(RD,EDX);
		if (opcode&0x2000000)
		{
			if (opcode&0x800000) { addbyte(0x41); addbyte(0x01); addbyte(0x47); addbyte(RN<<2); /*ADDL %eax,Rn*/ }
			else		     { addbyte(0x41); addbyte(0x29); addbyte(0x47); addbyte(RN<<2); /*SUBL %eax,Rn*/ }
		}
		else
		{
			if (templ)
			{
				if (opcode&0x800000) { addbyte(0x41); addbyte(0x81); addbyte(0x47); addbyte(RN<<2); addlong(templ); /*ADDL $temp,Rn*/ }
				else		     { addbyte(0x41); addbyte(0x81); addbyte(0x6F); addbyte(RN<<2); addlong(templ); /*SUBL $temp,Rn*/ }
			}
		}
		break;

	case 0x50: case 0x58: /* STR Rd, [Rn, # ]      */
	case 0x52: case 0x5a: /* STR Rd, [Rn, # ]!     */
	case 0x70: case 0x78: /* STR Rd, [Rn, reg...]  */
	case 0x72: case 0x7a: /* STR Rd, [Rn, reg...]! */
		if (RD==15) return 0;
		if (opcode&0x2000000) { if (!generateshift(opcode,pcpsr)) return 0; }
		else		      { addbyte(0xB8); addlong(opcode&0xFFF); /*MOVL $opcode&0xFFF,%eax*/ }
		genloadreggen(RN,EDI);
		if (RN==15) { addbyte(0x81); addbyte(0xE7); addlong(r15mask); /*ANDL $r15mask,%edi*/ }
		if (opcode&0x800000) { addbyte(0x01); addbyte(0xC7); /*ADDL %eax,%edi*/ }
		else		     { addbyte(0x29); addbyte(0xC7); /*SUBL %eax,%edi*/ }
		addbyte(0x57); /*PUSH %rdi*/
		addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edi*/
		genloadreg(RD);
		genstr();
		addbyte(0x5F); /*POP %rdi*/
		gentestabort();
		if (opcode&0x200000) /*Writeback*/
		{
			genstorereggen(RN,EDI);
		}
		break;

	case 0x54: case 0x5c: /* STRB Rd, [Rn, # ]      */
	case 0x56: case 0x5e: /* STRB Rd, [Rn, # ]!     */
	case 0x74: case 0x7c: /* STRB Rd, [Rn, reg...]  */
	case 0x76: case 0x7e: /* STRB Rd, [Rn, reg...]! */
		if (RD==15) return 0;
		if (opcode&0x2000000) { if (!generateshift(opcode,pcpsr)) return 0; }
		else		      { addbyte(0xB8); addlong(opcode&0xFFF); /*MOVL $opcode&0xFFF,%eax*/ }
		genloadreggen(RN,EDI);
		if (RN==15) { addbyte(0x81); addbyte(0xE7); addlong(r15mask); /*ANDL $r15mask,%edi*/ }
		if (opcode&0x800000) { addbyte(0x01); addbyte(0xC7); /*ADDL %eax,%edi*/ }
		else		     { addbyte(0x29); addbyte(0xC7); /*SUBL %eax,%edi*/ }
		genloadreg(RD);
		genstrb();
		gentestabort();
		if (opcode&0x200000) /*Writeback*/
		{
			genstorereggen(RN,EDI);
		}
		break;

	case 0x51: case 0x59: /* LDR Rd, [Rn, # ]      */
	case 0x53: case 0x5b: /* LDR Rd, [Rn, # ]!     */
	case 0x71: case 0x79: /* LDR Rd, [Rn, reg...]  */
	case 0x73: case 0x7b: /* LDR Rd, [Rn, reg...]! */
		if (RD==15) return 0;
		if (opcode&0x2000000) { if (!generateshift(opcode,pcpsr)) return 0; }
		else		      { addbyte(0xB8); addlong(opcode&0xFFF); /*MOVL $opcode&0xFFF,%eax*/ }
		genloadreggen(RN,EDI);
		if (RN==15) { addbyte(0x81); addbyte(0xE7); addlong(r15mask); /*ANDL $r15mask,%edi*/ }
		if (opcode&0x800000) { addbyte(0x01); addbyte(0xC7); /*ADDL %eax,%edi*/ }
		else		     { addbyte(0x29); addbyte(0xC7); /*SUBL %eax,%edi*/ }
		genldr();
		gentestabort();
		genstorereggen(RD,EDX);
		if (opcode&0x200000) /*Writeback*/
		{
			genstorereggen(RN,EDI);
		}
		break;

	case 0x55: case 0x5d: /* LDRB Rd, [Rn, # ]      */
	case 0x57: case 0x5f: /* LDRB Rd, [Rn, # ]!     */
	case 0x75: case 0x7d: /* LDRB Rd, [Rn, reg...]  */
	case 0x77: case 0x7f: /* LDRB Rd, [Rn, reg...]! */
		if (RD==15) return 0;
		if (opcode&0x2000000) { if (!generateshift(opcode,pcpsr)) return 0; }
		else		      { addbyte(0xB8); addlong(opcode&0xFFF); /*MOVL $opcode&0xFFF,%eax*/ }
		genloadreggen(RN,EDI);
		if (RN==15) { addbyte(0x81); addbyte(0xE7); addlong(r15mask); /*ANDL $r15mask,%edi*/ }
		if (opcode&0x800000) { addbyte(0x01); addbyte(0xC7); /*ADDL %eax,%edi*/ }
		else		     { addbyte(0x29); addbyte(0xC7); /*SUBL %eax,%edi*/ }
		genldrb();
		gentestabort();
		genstorereggen(RD,EDX);
		if (opcode&0x200000) /*Writeback*/
		{
			genstorereggen(RN,EDI);
		}
		break;

	case 0x80: /* STMDA */
	case 0x82: /* STMDA ! */
	case 0x90: /* STMDB */
	case 0x92: /* STMDB ! */
		if (RN==15) return 0;
		if (lastjumppos) return 0;
		genloadreggen(RN,EDI);
		addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edi*/
		for (c=15;c>=0;c--)
		{
			if (opcode&(1<<c))
			{
				genloadreg(c);
				if (opcode&0x1000000) { addbyte(0x83); addbyte(0xEF); addbyte(4); /*ADDL $4,%edi*/ }
				if (c==15) { addbyte(0x83); addbyte(0xC0); addbyte(4); /*ADD $4,%eax*/ }
				gen_x86_call(recompwritememl);
				if (!(opcode&0x1000000)) { addbyte(0x83); addbyte(0xEF); addbyte(4); /*ADDL $4,%edi*/ }
			}
		}
		gentestabort();
		if (opcode&0x200000)
		{
			genstorereggen(RN,EDI);
		}
		break;

	case 0x88: /* STMIA */
	case 0x8a: /* STMIA ! */
	case 0x98: /* STMIB */
	case 0x9a: /* STMIB ! */
		if (RN==15) return 0;
		if (lastjumppos) return 0;
		genloadreggen(RN,EDI);
		addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edi*/
		for (c=0;c<16;c++)
		{
			if (opcode&(1<<c))
			{
				genloadreg(c);
				if (opcode&0x1000000) { addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/ }
				if (c==15) { addbyte(0x83); addbyte(0xC0); addbyte(4); /*ADD $4,%eax*/ }
				gen_x86_call(recompwritememl);
				if (!(opcode&0x1000000)) { addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/ }
			}
		}
		gentestabort();
		if (opcode&0x200000)
		{
			genstorereggen(RN,EDI);
		}
		break;

	case 0x81: /* LDMDA */
	case 0x83: /* LDMDA ! */
	case 0x91: /* LDMDB */
	case 0x93: /* LDMDB ! */
		if (RN==15) return 0;
		if (lastjumppos) return 0;
		genloadreggen(RN,EDI);
		//if (opcode&0x1000000) { addbyte(0x83); addbyte(0xEF); addbyte(4); /*SUBL $4,%edi*/ }
		addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edi*/
		for (c=15;c>=0;c--)
		{
			if (opcode&(1<<c))
			{
				if (opcode&0x1000000) { addbyte(0x83); addbyte(0xEF); addbyte(4); /*SUBL $4,%edi*/ }
				gen_x86_call(recompreadmeml);
				if (!(opcode&0x1000000)) { addbyte(0x83); addbyte(0xEF); addbyte(4); /*SUBL $4,%edi*/ }
				if (c==15)
				{
					blockend=1;
					addbyte(0x83); addbyte(0xC2); addbyte(0x04); /*ADDL $4,%edx*/
					gentestabort();
				}
				if (c==15 && r15mask!=0xFFFFFFFC)
				{
					genloadreggen(15,ECX);
					addbyte(0x81); addbyte(0xE2); addlong(r15mask); /*AND $r15mask,%edx*/
					addbyte(0x81); addbyte(0xE1); addlong((~r15mask)); /*AND $~r15mask,%ecx*/
					addbyte(0x09); addbyte(0xCA); /*OR %ecx,%edx*/
				}
				genstorereggen(c,EDX);
			}
		}
		gentestabort();
		if (opcode&0x200000)
		{
			genstorereggen(RN,EDI);
		}
		break;

	case 0x89: /* LDMIA */
	case 0x8b: /* LDMIA ! */
	case 0x99: /* LDMIB */
	case 0x9b: /* LDMIB ! */
		if (RN==15) return 0;
		if (lastjumppos) return 0;
//		if (opcode&0x8000) return 0;
		//if (opcode&0x8000) { printf("R15 set!\n"); blockend=1; }
		genloadreggen(RN,EDI);
//		if (opcode&0x1000000) { addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/ }
		addbyte(0x83); addbyte(0xE7); addbyte(0xFC); /*ANDL $0xFFFFFFFC,%edi*/
		for (c=0;c<16;c++)
		{
			if (opcode&(1<<c))
			{
				if (opcode&0x1000000) { addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/ }
				gen_x86_call(recompreadmeml);
				if (!(opcode&0x1000000)) { addbyte(0x83); addbyte(0xC7); addbyte(4); /*ADDL $4,%edi*/ }
				if (c==15)
				{
					blockend=1;
					addbyte(0x83); addbyte(0xC2); addbyte(0x04); /*ADDL $4,%edx*/
					gentestabort();
				}
				if (c==15 && r15mask!=0xFFFFFFFC)
				{
					genloadreggen(15,ECX); /*MOVL R15,%ecx*/
					addbyte(0x81); addbyte(0xE2); addlong(r15mask); /*AND $r15mask,%edx*/
					addbyte(0x81); addbyte(0xE1); addlong((~r15mask)); /*AND $~r15mask,%ecx*/
					addbyte(0x09); addbyte(0xCA); /*OR %ecx,%edx*/
				}
				genstorereggen(c,EDX);
			}
		}
		if (!(opcode&0x8000)) gentestabort();
		if (opcode&0x200000)
		{
			genstorereggen(RN,EDI);
		}
		break;

	case 0xa0: case 0xa1: case 0xa2: case 0xa3: /* B */
	case 0xa4: case 0xa5: case 0xa6: case 0xa7:
	case 0xa8: case 0xa9: case 0xaa: case 0xab:
	case 0xac: case 0xad: case 0xae: case 0xaf:
		templ=(opcode&0xFFFFFF)<<2;
		if (templ&0x2000000) templ|=0xFC000000;
		templ+=4;
		if (!((PC+templ)&0xFC000000) || r15mask==0xFFFFFFFC)
		{
			/*ADD $templ,%r12d*/
			addbyte(0x41); addbyte(0x81); addbyte(0xC4);
			addlong(templ);
		}
		else
		{
			genloadreg(15);
			addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
			addbyte(0x81); addbyte(0xC0); addlong(templ); /*ADDL $templ,%eax*/
                        addbyte(0x81); addbyte(0xE2); addlong(0xFC000003); /*ANDL $templ,%edx*/
                        addbyte(0x81); addbyte(0xE0); addlong(0x03FFFFFC); /*ANDL $templ,%eax*/
                        addbyte(0x09); addbyte(0xD0); /*ORL %edx,%eax*/
			genstorereg(15);
		}
		blockend=1;
		break;

	case 0xb0: case 0xb1: case 0xb2: case 0xb3: /* BL */
	case 0xb4: case 0xb5: case 0xb6: case 0xb7:
	case 0xb8: case 0xb9: case 0xba: case 0xbb:
	case 0xbc: case 0xbd: case 0xbe: case 0xbf:
		templ=(opcode&0xFFFFFF)<<2;
		if (templ&0x2000000) templ|=0xFC000000;
		templ+=4;
		genloadreg(15);
		addbyte(0x83); addbyte(0xE8); addbyte(0x04); /*SUBL $4,%eax*/
		if (!((PC+templ)&0xFC000000) || r15mask==0xFFFFFFFC)
		{
			/*ADD $templ,%r12d*/
			addbyte(0x41); addbyte(0x81); addbyte(0xC4);
			addlong(templ);
			genstorereg(14);
		}
		else
		{
			genstorereg(14);
			addbyte(0x89); addbyte(0xC2); /*MOVL %eax,%edx*/
			addbyte(0x83); addbyte(0xC0); addbyte(0x04); /*ADDL $4,%eax*/
			addbyte(0x81); addbyte(0xE2); addlong(0xFC000003); /*ANDL $templ,%edx*/
			addbyte(0x81); addbyte(0xC0); addlong(templ); /*ADDL $templ,%eax*/
                        addbyte(0x81); addbyte(0xE0); addlong(0x03FFFFFC); /*ANDL $templ,%eax*/
                        addbyte(0x09); addbyte(0xD0); /*ORL %edx,%eax*/
			genstorereg(15);
		}
		blockend=1;
		break;

	default:
		return 0;
	}
	lastrecompiled=1;
	if (lastjumppos) rcodeblock[blockpoint2][lastjumppos]=(codeblockpos-lastjumppos)-1;
	return 1;
}

void generatecall(OpFn addr, uint32_t opcode,uint32_t *pcpsr)
{
//asm("addq $4,0x12345678;");
	lastrecompiled=0;
        tempinscount++;
	if (canrecompile[(opcode>>20)&0xFF])
	{
		if (recompile(opcode,pcpsr)) return;
	}
        //addbyte(0xC7); /*MOVL $opcode,(%esp)*/
        //addbyte(0x04);
        //addbyte(0x24);
	addbyte(0xBF); /*MOVL $opcode,%edi*/
        addlong(opcode);
	addbyte(0x45); addbyte(0x89); addbyte(0x67); addbyte(15<<2); /*MOVL %r12d,R15*/
        gen_x86_call(addr);
	addbyte(0x45); addbyte(0x8B); addbyte(0x67); addbyte(15<<2); /*MOVL R15,%r12d*/
//#if 0
        if (!flaglookup[opcode>>28][(*pcpsr)>>28] && (opcode&0xE000000)==0xA000000)
        {
//                rpclog("Carrying on - %i\n",pcinc);
//                generateupdatepc();
        if (pcinc)
        {
		addbyte(0x41); /*ADD $pcinc,%r12d*/
		addbyte(0x83);
		addbyte(0xC4);
		addbyte(pcinc);
//addbyte(0x67);
		//addbyte(0x41); /*ADD $4,armregs[15](%r15)*/
		//addbyte(0x83);
		//addbyte(0x47);
		//addbyte(15<<2); /*armregs[15]*/
		//addbyte(pcinc);

//                addbyte(0x83); /*ADD $4,armregs[15]*/
                //addbyte(0x04);
		//addbyte(0x25);
                //addlong(&armregs[15]);
                //addbyte(pcinc);
//                pcinc=0;
        }
                addbyte(0xE9); /*JMP 0*/
                addrel32(&rcodeblock[blockpoint2][0]);
        }
//        #endif
}
void generateupdatepc(void)
{
//asm("addl $4,0x12345678;");
        if (pcinc)
      {
		addbyte(0x41); /*ADD $pcinc,%r12d*/
		addbyte(0x83);
		addbyte(0xC4);
		addbyte(pcinc);
//		addbyte(0x41); /*ADD $4,armregs[15](%r15)*/
		//addbyte(0x83);
		//addbyte(0x47);
		//addbyte(15<<2); /*armregs[15]*/
		//addbyte(pcinc);
//                addbyte(0x83); /*ADD $4,armregs[15]*/
                //addbyte(0x04);
		//addbyte(0x25);
                //addlong(&armregs[15]);
                //addbyte(pcinc);
                pcinc=0;
        }
}
void generateupdateinscount(void)
{
        if (tempinscount)
        {
                addbyte(0x83); /*ADD tempinscount,inscount*/
                addbyte(0x04);
		addbyte(0x25);
                addlong(&inscount);
                addbyte(tempinscount);
                tempinscount=0;
        }
}

void generatepcinc(void)
{
	lastjumppos=0;
        pcinc+=4;
        if (pcinc==124) generateupdatepc();
        if (codeblockpos>=1200) blockend=1;
}

void removeblock(void)
{
        codeblockpc[blocknum]=0xFFFFFFFF;
        codeblocknum[blocknum]=0xFFFFFFFF;
}

int linecyc;

void endblock(uint32_t opcode, int c, uint32_t *pcpsr)
{
        /*asm("decl 0x12345678;");
	asm("testb $0xFF,0x12345678;");
asm("mov %eax,%edx;");
asm("and $0x12345678,%eax;");
asm("and $0x12345678,%edx;");
asm("cmp 0x12345678(%edx),%eax;");
asm("mov 0x12345678(%edx),%eax;");
asm("mov 0x12345678(,%eax,8),%rax;");
asm("jmp *%rax;");*/
/*asm("mov %rax,%rdx;");
asm("cmp 0x12345678(%rdx),%eax;");
asm("movl 0x12345678(%rdx),%eax;");
asm("movq 0x12345678(,%rax,8),%rax;");*/
//if (hasldrb[blockpoint2]) printf("ENding! %08X\n",r15mask);
        generateupdatepc();
        generateupdateinscount();
	addbyte(0x48); /*ADDL $8,%rsp*/
        addbyte(0x83);
        addbyte(0xC4);
        addbyte(0x08);

	addbyte(0x45); addbyte(0x89); addbyte(0x67); addbyte(15<<2); /*MOVL %r12d,R15*/

        if (c<128)
        {
                addbyte(0x83); /*ADDL $c,rinscount*/
                addbyte(0x04);
		addbyte(0x25);
                addlong(&rinscount);
                addbyte(c);
        }
        else
        {
                addbyte(0x81); /*ADDL $c,rinscount*/
                addbyte(0x04);
		addbyte(0x25);
                addlong(&rinscount);
                addlong(c);
        }

        addbyte(0xFF); /*DECL linecyc*/
        addbyte(0x0C);
	addbyte(0x25);
        addlong(&linecyc);
//addbyte(0xC3); /*RET*/

        addbyte(0x79); /*JNS +1*/
        addbyte(1);
        addbyte(0xC3); /*RET*/
        addbyte(0xF6); /*TESTB $0xFF,armirq*/
        addbyte(0x04);
	addbyte(0x25);
        addlong(&armirq);
        addbyte(0xFF);
        addbyte(0x75); /*JNZ*/
        addbyte(-11);

        genloadreg(15); /*MOVL armregs[15],%eax*/
        addbyte(0x83); /*SUBL $8,%eax*/
        addbyte(0xE8);
        addbyte(0x08);
	addbyte(0x48); /*MOVQ %rax,%rdx*/
        addbyte(0x89);
        addbyte(0xC2);
        //if (r15mask!=0xFFFFFFFC)
        //{
                addbyte(0x25); /*ANDL $r15mask,%eax*/
                addlong(r15mask);
        //}
        addbyte(0x81); /*ANDL $0x1FFFC,%edx*/
        addbyte(0xE2);
        addlong(0x1FFFC);
	//addbyte(0x67); /*CMPL codeblockpc[%edx],%eax*/
        addbyte(0x3B);
        addbyte(0x82);
        addlong(codeblockpc);
        addbyte(0x74); /*JZ +1*/
        addbyte(1);
        addbyte(0xC3); /*RET*/
        addbyte(0x8B); /*MOVL codeblocknum[%rdx],%eax*/
        addbyte(0x82);
        addlong(codeblocknum);
	addbyte(0x48); /*MOVL codeblockaddr[%rax*8],%rax*/
        addbyte(0x8B);
        addbyte(0x04);
        addbyte(0xC5);
        addlong(codeblockaddr);
        addbyte(0xFF); /*JMP *%rax*/
        addbyte(0xE0);
}

void dumplastblock(void)
{
/*        FILE *f=fopen("block.dmp","wb");
        fwrite(codeblock[blockcount][blocknum],1600,1,f);
        fclose(f);*/
}

/*int codecallblock(unsigned long l)
{
        int hash=HASH(l);
        void (*gen_func)(void);
        if (codeblockpc[0][hash]==l)
        {
                gen_func=(void *)(&codeblock[0][HASH(l)][1]);
                gen_func();
                return 1;
        }
        if (codeblockpc[1][hash]==l)
        {
                gen_func=(void *)(&codeblock[1][HASH(l)][1]);
                gen_func();
                return 1;
        }
        return 0;
}*/

void generateflagtestandbranch(uint32_t opcode, uint32_t *pcpsr)
{
//asm("movl 0x12345678,%eax;");
//asm("shrl $28,%eax;");
//asm("cmpb $0,0x12345678(%rax);");
        /*movl (pcpsr),%eax
          movl (opcode>>28)<<4,%edx
          shrl %eax,28
          or   %edx,%eax
          cmpb $0,(%eax,flaglookup)
          je skipins*/
	        if ((opcode>>28)==0xE) return; /*No need if 'always' condition code*/
        switch (opcode>>28)
        {
                case 0: /*EQ*/
                case 1: /*NE*/
		if ((pcpsr)==&armregs[15])
		{
			addbyte(0x41); /*TESTL $ZFLAG,%r12d*/
			addbyte(0xF7);
			addbyte(0xC4);
			addlong(0x40000000);
		}
		else
		{
	                addbyte(0xF6); /*TESTB (pcpsr>>24),$0x40*/
                	addbyte(0x04);
			addbyte(0x25);
                	addlong(((char *)pcpsr)+3);
                	addbyte(0x40);
		}
                if ((opcode>>28)&1) addbyte(0x75);                 /*JNE +5*/
                else                addbyte(0x74);                 /*JE +5*/
                break;
                case 2: /*CS*/
                case 3: /*CC*/
		if ((pcpsr)==&armregs[15])
		{
			addbyte(0x41); /*TESTL $CFLAG,%r12d*/
			addbyte(0xF7);
			addbyte(0xC4);
			addlong(0x20000000);
		}
		else
		{
                	addbyte(0xF6); /*TESTB (pcpsr>>24),$0x20*/
                	addbyte(0x04);
			addbyte(0x25);
	                addlong(((char *)pcpsr)+3);
	                addbyte(0x20);
		}
                if ((opcode>>28)&1) addbyte(0x75);                 /*JNE +5*/
                else                addbyte(0x74);                 /*JE +5*/
                break;
                case 4: /*MI*/
                case 5: /*PL*/
		if ((pcpsr)==&armregs[15])
		{
			addbyte(0x45); /*OR %r12d,%r12d*/
			addbyte(0x09);
			addbyte(0xE4);
                	if ((opcode>>28)&1) addbyte(0x78);                 /*JNS +5*/
                	else                addbyte(0x79);                 /*JS +5*/
		}
		else
		{
                	addbyte(0xF6); /*TESTB (pcpsr>>24),$0x80*/
                	addbyte(0x04);
			addbyte(0x25);
                	addlong(((char *)pcpsr)+3);
                	addbyte(0x80);
                	if ((opcode>>28)&1) addbyte(0x75);                 /*JNE +5*/
                	else                addbyte(0x74);                 /*JE +5*/
		}
                break;
                case 6: /*VS*/
                case 7: /*VC*/
		if ((pcpsr)==&armregs[15])
		{
			addbyte(0x41); /*TESTL $VFLAG,%r12d*/
			addbyte(0xF7);
			addbyte(0xC4);
			addlong(0x10000000);
		}
		else
		{
	                addbyte(0xF6); /*TESTB (pcpsr>>24),$0x10*/
                	addbyte(0x04);
			addbyte(0x25);
                	addlong(((char *)pcpsr)+3);
                	addbyte(0x10);
		}
                if ((opcode>>28)&1) addbyte(0x75);                 /*JNE +5*/
                else                addbyte(0x74);                 /*JE +5*/
                break;
                default:
		if ((pcpsr)==&armregs[15])
		{
			genloadreg(15);
		}
		else
		{
	                addbyte(0x8B);                 /*MOVL (pcpsr),%eax*/
			addbyte(0x04);
			addbyte(0x25);
	                addlong((char *)pcpsr);
		}
                addbyte(0xC1);                 /*SHRL $28,%eax*/
                addbyte(0xE8);
                addbyte(0x1C);
                addbyte(0x80);                 /*CMPB $0,flaglookup(%eax)*/
                addbyte(0xB8);
                addlong((char *)(&flaglookup[opcode>>28][0]));
                addbyte(0);
                addbyte(0x74);                 /*JE +5*/
                break;
        }
	lastjumppos=codeblockpos;
//        if (output) rpclog("PC %07X - %08X  %i\n",PC,opcode,(((opcode+0x6000000)&0xF000000)>0xA000000));
        if (!flaglookup[opcode>>28][(*pcpsr)>>28] && (opcode&0xE000000)==0xA000000) addbyte(5+5+5+((pcinc)?4:0)+8);
#ifdef ABORTCHECKING
        else if (((opcode+0x6000000)&0xF000000)>=0xA000000)
        {
                if (((char *)(&rcodeblock[blockpoint2][codeblockpos+17+8])-(char *)(&rcodeblock[blockpoint2][0]))<120) addbyte(5+5+2+2+8);
                else                                                                                                     addbyte(5+5+2+6+8);
        }
#endif
        else                                                                        addbyte(5+5+8);
}
void generateirqtest(void)
{
	if (lastrecompiled) { lastrecompiled=0; return; }
        //asm("testl %eax,%eax");
//asm("jne 0");
//        asm("testb $0xC0,0x12345678");
//        addbyte(0xF6); /*TESTB $0x40,armirq*/
//        addbyte(0x05);
//        addlong(&armirq);
//        addbyte(0x40);
        addbyte(0x85); /*TESTL %eax,%eax*/
        addbyte(0xC0);
//        #if 0
        if (((char *)(&rcodeblock[blockpoint2][codeblockpos+4])-(char *)(&rcodeblock[blockpoint2][0]))<120)
        {
                addbyte(0x75); /*JNE 0*/
                addbyte((char *)&rcodeblock[blockpoint2][0]-(char *)(&rcodeblock[blockpoint2][codeblockpos+1]));
        }
        else
        {
//                #endif
                addbyte(0x0F); /*JNE 0*/
                addbyte(0x85);
                addrel32(&rcodeblock[blockpoint2][0]);
        }
}
#endif
#endif
