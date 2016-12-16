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

#ifndef __CMOS__
#define __CMOS__

extern void loadcmos(void);
extern void savecmos(void);
extern void reseti2c(uint32_t chosen_i2c_devices);
extern void cmosi2cchange(int nuclock, int nudata);

extern int i2cclock;
extern int i2cdata;

/** Values used in bitfield of I2C devices */
#define I2C_PCF8583	(1 << 0)
#define I2C_SPD_DIMM0	(1 << 1)

#endif //__CMOS__
