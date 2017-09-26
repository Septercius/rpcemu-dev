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

#ifndef __KEYBOARD__
#define __KEYBOARD__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern void keyboard_reset(void);
extern void keyboard_callback_rpcemu(void);
extern void mouse_ps2_callback(void);
extern void keyboard_data_write(uint8_t v);
extern void keyboard_control_write(uint8_t v);
extern void mouse_data_write(uint8_t v);
extern void mouse_control_write(uint8_t v);
extern uint8_t keyboard_status_read(void);
extern uint8_t keyboard_data_read(void);
extern uint8_t mouse_status_read(void);
extern uint8_t mouse_data_read(void);
extern void keyboard_poll(void);
extern void keyboard_key_press(const uint8_t *);
extern void keyboard_key_release(const uint8_t *);
extern const uint8_t *keyboard_map_key(uint32_t);
extern int mouse_buttons_get(void);


extern void mouse_mouse_move(int x, int y);
extern void mouse_mouse_move_relative(int dx, int dy);
extern void mouse_mouse_press(int buttons);
extern void mouse_mouse_release(int buttons);

extern void mouse_hack_osword_21_0(uint32_t a);
extern void mouse_hack_osword_21_1(uint32_t a);
extern void mouse_hack_osword_21_4(uint32_t a);
extern void mouse_hack_osbyte_106(uint32_t a);
extern void mouse_hack_osmouse(void);
extern void mouse_hack_get_pos(int *x, int *y);

extern int mouse_b;

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif //__KEYBOARD__
