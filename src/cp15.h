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

#ifndef __CP15__
#define __CP15__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern void cp15_tlb_invalidate_physical(uint32_t addr);

extern void cp15_reset(CPUModel cpu_model);
extern void cp15_init(void);

extern void cp15_write(uint32_t opcode, uint32_t val);
extern uint32_t cp15_read(uint32_t opcode);

extern const uint32_t *getpccache(uint32_t addr);
extern uint32_t translateaddress2(uint32_t addr, int rw, int prefetch);

extern int flushes;
extern int tlbs;
extern int dcache;

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif //__CP15__
