#ifndef __CP15__
#define __CP15__

extern void cp15_reset(CPUModel cpu_model);
extern void cp15_init(void);

extern void cp15_write(uint32_t addr, uint32_t val, uint32_t opcode);
extern uint32_t cp15_read(uint32_t addr);

extern const uint32_t *getpccache(uint32_t addr);
extern uint32_t translateaddress2(uint32_t addr, int rw, int prefetch);

extern int flushes;
extern int tlbs;
extern int dcache;

#endif //__CP15__
