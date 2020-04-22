/*
 RPCEmu - An Acorn system emulator
 
 Copyright (C) 2017 Matthew Howkins
 
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

#ifndef __KEYBOARD_MACOSX_H__
#define __KEYBOARD_MACOSX_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    
extern void keyboard_handle_modifier_keys(uint32_t mask);
extern void keyboard_reset_modifiers(int sendReleaseEvent);
extern void keyboard_configure_layout(const char *layoutName);
extern int keyboard_check_special_keys();
    
#ifdef __cplusplus
}
#endif /* __cplusplus */
        
#endif

