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

#ifndef __VIDC20__
#define __VIDC20__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* VIDC modes that are too small are scaled up for host OS.
   Also handles stretching small rectangular pixel modes */
#define VIDC_DOUBLE_NONE	0
#define VIDC_DOUBLE_X		1
#define VIDC_DOUBLE_Y		2
#define VIDC_DOUBLE_BOTH	3

extern void initvideo(void);
extern void closevideo(void);
extern int vidc_get_xsize(void);
extern int vidc_get_ysize(void);
extern void resetbuffer(void);
extern void writevidc20(uint32_t val);
extern void drawscr(int needredraw);
extern void togglefullscreen(int fs);
extern void vidcthread(void);
extern void vidc_get_doublesize(int *double_x, int *double_y);

/* Platform specific functions */
extern void vidcstartthread(void);
extern void vidcendthread(void);
extern void vidcwakeupthread(void);
extern int vidctrymutex(void);
extern void vidcreleasemutex(void);

extern int fullscreen;

extern uint8_t *dirtybuffer;

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


#endif //__VIDC20__
