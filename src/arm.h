#ifndef __ARM__
#define __ARM__

extern void updatemode(uint32_t m);
extern void resetcodeblocks();
extern void initcodeblocks();
extern void generatepcinc();
extern void generateupdatepc();
extern void generateupdateinscount();
extern void generateflagtestandbranch(uint32_t opcode, uint32_t *pcpsr);
extern void generatecall(unsigned long addr, uint32_t opcode, uint32_t *pcpsr);
extern void generateirqtest();
extern void endblock(int c);
extern void initcodeblock(uint32_t l);

extern uint32_t *usrregs[16],userregs[17],superregs[17],fiqregs[17],irqregs[17],abortregs[17],undefregs[17],systemregs[17];
extern uint32_t spsr[16];
extern uint32_t armregs[17];
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
#endif //__ARM__
