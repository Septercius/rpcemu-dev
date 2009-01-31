#ifndef __ARM__
#define __ARM__

typedef void (*OpFn)(uint32_t opcode);

extern void updatemode(uint32_t m);
extern void resetcodeblocks();
extern void initcodeblocks();
extern void generatepcinc();
extern void generateupdatepc();
extern void generateupdateinscount();
extern void generateflagtestandbranch(uint32_t opcode, uint32_t *pcpsr);
extern void generatecall(OpFn addr, uint32_t opcode, uint32_t *pcpsr);
extern void generateirqtest();
extern void endblock(int c, uint32_t *pcpsr);
extern void initcodeblock(uint32_t l);
extern int codewritememfb();
extern uint32_t *usrregs[16];
extern uint32_t armregs[18];
extern int armirq; //,armfiq;
extern int cpsr;
#ifdef PREFETCH
#define PC (armregs[15]&r15mask)
#else
#define PC ((armregs[15]-8)&r15mask)
#endif
extern uint32_t ins,output;
extern int r15mask;
extern uint32_t mode;
extern int irq;
extern unsigned char flaglookup[16][16];
extern void resetarm(void);
extern void execarm(int cycles);
extern void dumpregs(void);
extern void exception(int mmode, uint32_t address, int diff);

extern int databort,prefabort;
extern int prog32;
extern int indumpregs;
extern int blockend;

extern uint32_t oldpc,oldpc2,oldpc3;

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

#endif //__ARM__
