#ifndef FDC_H
#define FDC_H

extern void fdc_reset(void);
extern void fdc_callback(void);
extern uint8_t fdc_dma_read(uint32_t addr);
extern void fdc_dma_write(uint32_t addr, uint8_t val);
extern void fdc_adf_load(const char *fn, int drive);
extern void fdc_adf_save(const char *fn, int drive);
extern uint8_t fdc_read(uint32_t addr);
extern void fdc_write(uint32_t addr, uint32_t val);

extern int fdccallback;
extern int motoron;

#endif /* FDC_H */
