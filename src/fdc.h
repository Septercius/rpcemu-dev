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

#ifndef FDC_H
#define FDC_H

extern void fdc_reset(void);
extern void fdc_init(void);
extern void fdc_callback(void);
extern uint8_t fdc_dma_read(uint32_t addr);
extern void fdc_dma_write(uint32_t addr, uint8_t val);
extern void fdc_image_load(const char *fn, int drive);
extern void fdc_image_save(const char *fn, int drive);
extern uint8_t fdc_read(uint32_t addr);
extern void fdc_write(uint32_t addr, uint32_t val);

extern int fdccallback;
extern int motoron;

extern void fdc_data(uint8_t dat);
extern void fdc_finishread(void);
extern void fdc_notfound(void);
extern void fdc_datacrcerror(void);
extern void fdc_headercrcerror(void);
extern void fdc_writeprotect(void);
extern int fdc_getdata(int last);
extern void fdc_sectorid(uint8_t track, uint8_t side, uint8_t sector, uint8_t size);
extern void fdc_indexpulse(void);

#endif /* FDC_H */
