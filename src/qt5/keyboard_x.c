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
#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint32_t	native_scancode;	// Native Scancode (X11)
	uint8_t		set_2[8];		// PS/2 Set 2 make code
} KeyMapInfo;

static const KeyMapInfo key_map[] = {
	{ 0x09, { 0x76 } },		// Escape
	{ 0x0a, { 0x16 } },		// 1
	{ 0x0b, { 0x1e } },		// 2
	{ 0x0c, { 0x26 } },		// 3
	{ 0x0d, { 0x25 } },		// 4
	{ 0x0e, { 0x2e } },		// 5
	{ 0x0f, { 0x36 } },		// 6
	{ 0x10, { 0x3d } },		// 7
	{ 0x11, { 0x3e } },		// 8
	{ 0x12, { 0x46 } },		// 9
	{ 0x13, { 0x45 } },		// 0
	{ 0x14, { 0x4e } },		// -
	{ 0x15, { 0x55 } },		// =
	{ 0x16, { 0x66 } },		// Backspace

	{ 0x17, { 0x0d } },		// Tab
	{ 0x18, { 0x15 } },		// Q
	{ 0x19, { 0x1d } },		// W
	{ 0x1a, { 0x24 } },		// E
	{ 0x1b, { 0x2d } },		// R
	{ 0x1c, { 0x2c } },		// T
	{ 0x1d, { 0x35 } },		// Y
	{ 0x1e, { 0x3c } },		// U
	{ 0x1f, { 0x43 } },		// I
	{ 0x20, { 0x44 } },		// O
	{ 0x21, { 0x4d } },		// P
	{ 0x22, { 0x54 } },		// [
	{ 0x23, { 0x5b } },		// ]
	{ 0x24, { 0x5a } },		// Return

	{ 0x25, { 0x14 } },		// Left Ctrl
	{ 0x26, { 0x1c } },		// A
	{ 0x27, { 0x1b } },		// S
	{ 0x28, { 0x23 } },		// D
	{ 0x29, { 0x2b } },		// F
	{ 0x2a, { 0x34 } },		// G
	{ 0x2b, { 0x33 } },		// H
	{ 0x2c, { 0x3b } },		// J
	{ 0x2d, { 0x42 } },		// K
	{ 0x2e, { 0x4b } },		// L
	{ 0x2f, { 0x4c } },		// ;
	{ 0x30, { 0x52 } },		// '
	{ 0x31, { 0x0e } },		// `

	{ 0x32, { 0x12 } },		// Left Shift
	{ 0x33, { 0x5d } },		// # (International only)
	{ 0x34, { 0x1a } },		// Z
	{ 0x35, { 0x22 } },		// X
	{ 0x36, { 0x21 } },		// C
	{ 0x37, { 0x2a } },		// V
	{ 0x38, { 0x32 } },		// B
	{ 0x39, { 0x31 } },		// N
	{ 0x3a, { 0x3a } },		// M
	{ 0x3b, { 0x41 } },		// ,
	{ 0x3c, { 0x49 } },		// .
	{ 0x3d, { 0x4a } },		// /
	{ 0x3e, { 0x59 } },		// Right Shift
	{ 0x3f, { 0x7c } },		// Keypad *

	{ 0x40, { 0x11 } },		// Left Alt
	{ 0x41, { 0x29 } },		// Space
	{ 0x42, { 0x58 } },		// Caps Lock

	{ 0x43, { 0x05 } },		// F1
	{ 0x44, { 0x06 } },		// F2
	{ 0x45, { 0x04 } },		// F3
	{ 0x46, { 0x0c } },		// F4
	{ 0x47, { 0x03 } },		// F5
	{ 0x48, { 0x0b } },		// F6
	{ 0x49, { 0x83 } },		// F7
	{ 0x4a, { 0x0a } },		// F8
	{ 0x4b, { 0x01 } },		// F9
	{ 0x4c, { 0x09 } },		// F10

	{ 0x4d, { 0x77 } },		// Keypad Num Lock
	{ 0x4e, { 0x7e } },		// Scroll Lock
	{ 0x4f, { 0x6c } },		// Keypad 7
	{ 0x50, { 0x75 } },		// Keypad 8
	{ 0x51, { 0x7d } },		// Keypad 9
	{ 0x52, { 0x7b } },		// Keypad -
	{ 0x53, { 0x6b } },		// Keypad 4
	{ 0x54, { 0x73 } },		// Keypad 5
	{ 0x55, { 0x74 } },		// Keypad 6
	{ 0x56, { 0x79 } },		// Keypad +
	{ 0x57, { 0x69 } },		// Keypad 1
	{ 0x58, { 0x72 } },		// Keypad 2
	{ 0x59, { 0x7a } },		// Keypad 3
	{ 0x5a, { 0x70 } },		// Keypad 0
	{ 0x5b, { 0x71 } },		// Keypad .

	{ 0x5e, { 0x61 } },		// Backslash (International only)
	{ 0x5f, { 0x78 } },		// F11
	{ 0x60, { 0x07 } },		// F12

	{ 0x68, { 0xe0, 0x5a } },	// Keypad Enter
	{ 0x69, { 0xe0, 0x14 } },	// Right Ctrl
	{ 0x6a, { 0xe0, 0x4a } },	// Keypad /

	{ 0x6e, { 0xe0, 0x6c } },	// Home
	{ 0x6f, { 0xe0, 0x75 } },	// Up
	{ 0x70, { 0xe0, 0x7d } },	// Page Up
	{ 0x71, { 0xe0, 0x6b } },	// Left
	{ 0x72, { 0xe0, 0x74 } },	// Right
	{ 0x73, { 0xe0, 0x69 } },	// End
	{ 0x74, { 0xe0, 0x72 } },	// Down
	{ 0x75, { 0xe0, 0x7a } },	// Page Down
	{ 0x76, { 0xe0, 0x70 } },	// Insert
	{ 0x77, { 0xe0, 0x71 } },	// Delete
	{ 0x7f, { 0xe1, 0x14, 0x77, 0xe1, 0xf0, 0x14, 0xf0, 0x77 } },	// Break

	{ 0x85, { 0xe0, 0x1f } },	// Left Win
	{ 0x86, { 0xe0, 0x27 } },	// Right Win
	{ 0x87, { 0xe0, 0x2f } },	// Appication (Win Menu)

	{ 0, { 0, 0 } },
};

const uint8_t *
keyboard_map_key(uint32_t native_scancode)
{
	size_t k;

	for (k = 0; key_map[k].native_scancode != 0; k++) {
		if (key_map[k].native_scancode == native_scancode) {
			return key_map[k].set_2;
		}
	}
	return NULL;
}
