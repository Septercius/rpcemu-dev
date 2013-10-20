#ifndef SUPERIO_H
#define SUPERIO_H

/**
 * SuperIO chip type.
 */
typedef enum {
	SuperIOType_FDC37C665GT,
	SuperIOType_FDC37C672
} SuperIOType;

extern void superio_reset(SuperIOType chosen_super_type);
extern uint8_t superio_read(uint32_t addr);
extern void superio_write(uint32_t addr, uint32_t val);

extern void superio_smi_setint1(uint8_t i);
extern void superio_smi_setint2(uint8_t i);
extern void superio_smi_clrint1(uint8_t i);
extern void superio_smi_clrint2(uint8_t i);

#endif /* SUPERIO_H */
