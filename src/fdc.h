#ifndef FDC_H
#define FDC_H

extern void fdc_reset(void);
extern void callbackfdc(void);
extern uint8_t readfdcdma(uint32_t addr);
extern void writefdcdma(uint32_t addr, uint8_t val);
extern void loadadf(const char *fn, int drive);
extern void saveadf(const char *fn, int drive);
extern uint8_t readfdc(uint32_t addr);
extern void writefdc(uint32_t addr, uint32_t val);

extern int fdccallback;
extern int motoron;

#endif /* FDC_H */
