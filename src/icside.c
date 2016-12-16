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

#include <stdio.h>
#include <string.h>

#include "rpcemu.h"
#include "podules.h"
#include "ide.h"

unsigned char icsrom[8192];
int icspage;

uint8_t icsreadb(podule *p, int easi, uint32_t addr)
{
	NOT_USED(p);
	NOT_USED(easi);
	NOT_USED(addr);
/*        int temp;
//        rpclog("Read ICSB %04X\n",addr);
        switch (addr&0x3000)
        {
                case 0x0000: case 0x1000:
                temp=((addr&0x1FFC)|(icspage<<13))>>2;
                return icsrom[temp];
                case 0x3000:
                return readide(((addr>>2)&7)+0x1F0);
        }*/
	return 0;
}

uint16_t icsreadw(podule *p, int easi, uint32_t addr)
{
	NOT_USED(p);
	NOT_USED(easi);
	NOT_USED(addr);
/*        if ((addr&0x3000)==0x3000)
        {
//                rpclog("Read IDEW\n");
                return readidew();
        }
        return icsreadb(p,easi,addr);*/
	return 0;
}

void icswriteb(podule *p, int easi, uint32_t addr, uint8_t val)
{
	NOT_USED(p);
	NOT_USED(easi);
	NOT_USED(addr);
	NOT_USED(val);
//        rpclog("Write ICSB %04X %02X\n",addr,val);
/*        switch (addr&0x3000)
        {
                case 0x2000: icspage=val; return;
                case 0x3000:
                writeide(((addr>>2)&7)+0x1F0,val);
                return;
        }*/
}

void icswritew(podule *p, int easi, uint32_t addr, uint16_t val)
{
	NOT_USED(p);
	NOT_USED(easi);
	NOT_USED(addr);
	NOT_USED(val);
/*        if ((addr&0x3000)==0x3000)
        {
//                rpclog("WRITEIDEW\n");
                return writeidew(val);
        }
        icswriteb(p,easi,addr,val);*/
}

void initics(void)
{
/*        FILE *f;
        char fn[512];

	snprintf(fn, sizeof(fn), "%szidefs", rpcemu_get_datadir());
        f=fopen(fn,"rb");
        if (!f)
        {
                rpclog("Failed to open ZIDEFS!\n");
                return;
        }
        fread(icsrom,8192,1,f);
        fclose(f);
        addpodule(NULL,icswritew,icswriteb,NULL,icsreadw,icsreadb,NULL,NULL);*/
//        rpclog("ICS Initialised!\n");
}
