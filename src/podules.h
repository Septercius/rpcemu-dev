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

#ifndef PODULES_H
#define PODULES_H

void writepodulel(int num, int easi, uint32_t addr, uint32_t val);
void writepodulew(int num, int easi, uint32_t addr, uint32_t val);
void writepoduleb(int num, int easi, uint32_t addr, uint8_t val);
uint32_t  readpodulel(int num, int easi, uint32_t addr);
uint32_t readpodulew(int num, int easi, uint32_t addr);
uint8_t  readpoduleb(int num, int easi, uint32_t addr);

typedef struct podule
{
        void (*writeb)(struct podule *p, int easi, uint32_t addr, uint8_t val);
        void (*writew)(struct podule *p, int easi, uint32_t addr, uint16_t val);
        void (*writel)(struct podule *p, int easi, uint32_t addr, uint32_t val);
        uint8_t  (*readb)(struct podule *p, int easi, uint32_t addr);
        uint16_t (*readw)(struct podule *p, int easi, uint32_t addr);
        uint32_t (*readl)(struct podule *p, int easi, uint32_t addr);
        int (*timercallback)(struct podule *p);
        void (*reset)(struct podule *p);
        int irq,fiq;
        int msectimer;
        int broken;
} podule;

void rethinkpoduleints(void);

podule *addpodule(void (*writel)(podule *p, int easi, uint32_t addr, uint32_t val),
              void (*writew)(podule *p, int easi, uint32_t addr, uint16_t val),
              void (*writeb)(podule *p, int easi, uint32_t addr, uint8_t val),
              uint32_t (*readl)(podule *p, int easi, uint32_t addr),
              uint16_t (*readw)(podule *p, int easi, uint32_t addr),
              uint8_t  (*readb)(podule *p, int easi, uint32_t addr),
              int (*timercallback)(podule *p),
              void (*reset)(podule *p),
              int broken);

void runpoduletimers(int t);
void podules_reset(void);

#endif
