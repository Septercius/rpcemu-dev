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

#ifndef I8042_H
#define I8042_H

void i8042_keyboard_irq_raise(void);
void i8042_keyboard_irq_lower(void);
void i8042_mouse_irq_raise(void);
void i8042_mouse_irq_lower(void);

uint8_t i8042_data_read(void);
void i8042_data_write(uint8_t val);

uint8_t i8042_status_read(void);
void i8042_command_write(uint8_t val);

void i8042_reset(void);

#endif /* I8042_H */
