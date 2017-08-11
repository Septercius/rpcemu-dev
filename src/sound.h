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

#ifndef __SOUND__
#define __SOUND__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern void sound_init(void);

extern void sound_restart(void);
extern void sound_pause(void);

extern void sound_samplefreq_change(int newsamplefreq);
extern void sound_irq_update(void);
extern void sound_buffer_update(void);

extern int soundbufferfull;
extern uint32_t soundaddr[4];
extern int soundinited, soundlatch, soundcount;

/* Provide by platform specific code */
extern void plt_sound_init(uint32_t bufferlen);
extern void plt_sound_restart(void);
extern void plt_sound_pause(void);
extern int32_t plt_sound_buffer_free(void);
extern void plt_sound_buffer_play(const char *buffer, uint32_t length);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif //__SOUND__
