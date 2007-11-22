#ifndef PODULEROM_H
#define PODULEROM_H

#include <stdint.h>
#include "podules.h"

void initpodulerom(void);

uint8_t readpodulerom(podule *p, int easi, uint32_t addr);

#endif
