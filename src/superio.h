/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2005-2010 Sarah Walker

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

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
