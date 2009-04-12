#ifndef __82C711__
#define __82C711__

extern void reset82c711(void);
extern uint8_t read82c711(uint32_t addr);
extern void write82c711(uint32_t addr, uint32_t val);

#endif //__82C711__
