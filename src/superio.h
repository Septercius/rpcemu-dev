#ifndef SUPERIO_H
#define SUPERIO_H

extern void superio_reset(void);
extern uint8_t superio_read(uint32_t addr);
extern void superio_write(uint32_t addr, uint32_t val);

#endif /* SUPERIO_H */
