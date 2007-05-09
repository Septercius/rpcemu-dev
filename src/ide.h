#ifndef __IDE__
#define __IDE__

extern void writeide(uint16_t addr, uint8_t val);
extern void writeidew(uint16_t val);
extern uint8_t readide(uint16_t addr);
extern uint16_t readidew(void);
extern void callbackide(void);
extern void resetide(void);

#endif //__IDE__
