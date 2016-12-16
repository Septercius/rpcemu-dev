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

#ifndef __IDE__
#define __IDE__

extern void writeide(uint16_t addr, uint8_t val);
extern void writeidew(uint16_t val);
extern uint8_t readide(uint16_t addr);
extern uint16_t readidew(void);
extern void callbackide(void);
extern void resetide(void);

/*ATAPI stuff*/
typedef struct ATAPI
{
        int (*ready)(void);
        int (*readtoc)(unsigned char *b, unsigned char starttrack, int msf);
        uint8_t (*getcurrentsubchannel)(uint8_t *b, int msf);
        void (*readsector)(uint8_t *b, int sector);
        void (*playaudio)(uint32_t pos, uint32_t len);
        void (*seek)(uint32_t pos);
        void (*load)(void);
        void (*eject)(void);
        void (*pause)(void);
        void (*resume)(void);
        void (*stop)(void);
        void (*exit)(void);
} ATAPI;

extern ATAPI *atapi;
extern int idecallback;

void atapi_discchanged(void);

#endif //__IDE__
