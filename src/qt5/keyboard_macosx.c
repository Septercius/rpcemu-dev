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

#include "rpcemu.h"
#include "keyboard.h"

#include <Carbon/Carbon.h>

#define UNUSED(x) (void)(x)

const int MAX_KEYBOARD_LAYOUTS = 20;

typedef enum
{
    keyboardLayoutUndefined = 0,
    keyboardLayoutBritish = 1,
    keyboardLayoutFrench = 2
} KeyboardLayoutType;

static int keyboardType;

typedef struct {
    uint32_t    virtual_key[MAX_KEYBOARD_LAYOUTS];      // Cocoa virtual keys
    uint8_t     set_2[8];                               // PS/2 Set 2 make code
} KeyMapInfo;

// Mac virtual keys can be found in the following file:
//
// /System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/Headers/Events.h.
//
// Key mappings are defined as follows:
//
// The first member is an array of virtual key codes.  There will be at least three elements in the array for each key.
//
// The first element indicates whether or not there are different mappings for different keyboard layouts for this key code.
// If the value is 0, each keyboard layout uses the same mapping.  Where the value is 1, there are different mappings for different layouts.
// An example of the former is "0" and of the latter, "Z" (in French, this is "Y").
//
// The second element in the array is the virtual key to use for the default language, British.
// If additional, non-British languages are defined in the 'KeyboardLayoutType' enumeration (above) and in the
// 'configureKeyboardLayout' function (below), virtual keys for these languages can be specified.
// For example, on a French keyboard, 'Y' and 'Z' are transposed.  Therefore, for each of the mappings for these keys,
// two virtual keys are listed.
//
// The list of virtual key codes must be terminated with an 0xFFFF element.
//
// The second member is an array of PS/2 set 2 codes.

static const KeyMapInfo key_map[] = {
    { { 0, kVK_Escape, 0xFFFF }, { 0x76 } },                       // Escape

    { { 0, kVK_ISO_Section, 0xFFFF }, { 0x0e } },                  // `
    { { 0, kVK_ANSI_1, 0xFFFF}, { 0x16 } },                        // 1
    { { 0, kVK_ANSI_2, 0xFFFF }, { 0x1e } },                       // 2
    { { 0, kVK_ANSI_3, 0xFFFF}, { 0x26 } },                        // 3
    { { 0, kVK_ANSI_4, 0xFFFF }, { 0x25 } },                       // 4
    { { 0, kVK_ANSI_5, 0xFFFF }, { 0x2e } },                       // 5
    { { 0, kVK_ANSI_6, 0xFFFF }, { 0x36 } },                       // 6
    { { 0, kVK_ANSI_7, 0xFFFF }, { 0x3d } },                       // 7
    { { 0, kVK_ANSI_8, 0xFFFF }, { 0x3e } },                       // 8
    { { 0, kVK_ANSI_9, 0xFFFF }, { 0x46 } },                       // 9
    { { 0, kVK_ANSI_0, 0xFFFF }, { 0x45 } },                       // 0
    { { 0, kVK_ANSI_Minus, 0xFFFF }, { 0x4e } },                   // -
    { { 0, kVK_ANSI_Equal, 0xFFFF }, { 0x55 } },                   // =
    { { 0, kVK_Delete, 0xFFFF }, { 0x66 } },                       // Backspace

    { { 0, kVK_Tab, 0xFFFF }, { 0x0d } },                          // Tab
    { { 0, kVK_ANSI_Q, 0xFFFF }, { 0x15 } },                       // Q
    { { 0, kVK_ANSI_W, 0xFFFF }, { 0x1d } },                       // W
    { { 0, kVK_ANSI_E, 0xFFFF }, { 0x24 } },                       // E
    { { 0, kVK_ANSI_R, 0xFFFF }, { 0x2d } },                       // R
    { { 0, kVK_ANSI_T, 0xFFFF}, { 0x2c } },                        // T
    { { 1, kVK_ANSI_Y, kVK_ANSI_Z, 0xFFFF }, { 0x35 } },           // Y
    { { 0, kVK_ANSI_U, 0xFFFF }, { 0x3c } },                       // U
    { { 0, kVK_ANSI_I, 0xFFFF }, { 0x43 } },                       // I
    { { 0, kVK_ANSI_O, 0xFFFF }, { 0x44 } },                       // O
    { { 0, kVK_ANSI_P, 0xFFFF }, { 0x4d } },                       // P
    { { 0, kVK_ANSI_LeftBracket, 0xFFFF }, { 0x54 } },             // [
    { { 0, kVK_ANSI_RightBracket, 0xFFFF }, { 0x5b } },            // ]
    { { 0, kVK_Return, 0xFFFF }, { 0x5a } },                       // Return

    { { 0, kVK_Control, 0xFFFF }, { 0x14 } },                      // Left Ctrl
    { { 0, kVK_ANSI_A, 0xFFFF }, { 0x1c } },                       // A
    { { 0, kVK_ANSI_S, 0xFFFF }, { 0x1b } },                       // S
    { { 0, kVK_ANSI_D, 0xFFFF }, { 0x23 } },                       // D
    { { 0, kVK_ANSI_F, 0xFFFF }, { 0x2b } },                       // F
    { { 0, kVK_ANSI_G, 0xFFFF }, { 0x34 } },                       // G
    { { 1, kVK_ANSI_H, 0xFFFF }, { 0x33 } },                       // H
    { { 0, kVK_ANSI_J, 0xFFFF }, { 0x3b } },                       // J
    { { 0, kVK_ANSI_K, 0xFFFF }, { 0x42 } },                       // K
    { { 0, kVK_ANSI_L, 0xFFFF }, { 0x4b } },                       // L
    { { 0, kVK_ANSI_Semicolon, 0xFFFF }, { 0x4c } },               // ;
    { { 0, kVK_ANSI_Quote, 0xFFFF }, { 0x52 } },                   // '
    { { 0, kVK_ANSI_Backslash, 0xFFFF }, { 0x5d } },               // # (International only)

    { { 0, kVK_ANSI_Grave, 0xFFFF }, { 0x61 } },                   // `
    { { 1, kVK_ANSI_Z, kVK_ANSI_Y, 0xFFFF }, { 0x1a } },           // Z
    { { 0, kVK_ANSI_X, 0xFFFF }, { 0x22 } },                       // X
    { { 0, kVK_ANSI_C, 0xFFFF }, { 0x21 } },                       // C
    { { 0, kVK_ANSI_V, 0xFFFF }, { 0x2a } },                       // V
    { { 0, kVK_ANSI_B, 0xFFFF }, { 0x32 } },                       // B
    { { 0, kVK_ANSI_N, 0xFFFF }, { 0x31 } },                       // N
    { { 0, kVK_ANSI_M, 0xFFFF }, { 0x3a } },                       // M
    { { 0, kVK_ANSI_Comma, 0xFFFF }, { 0x41 } },                   // ,
    { { 0, kVK_ANSI_Period, 0xFFFF }, { 0x49 } },                  // .
    { { 0, kVK_ANSI_Slash, 0xFFFF }, { 0x4a } },                   // /

    { { 0, kVK_Space, 0xFFFF }, { 0x29 } },                        // Space

    { { 0, kVK_F1, 0xFFFF }, { 0x05 } },                           // F1
    { { 0, kVK_F2, 0xFFFF }, { 0x06 } },                           // F2
    { { 0, kVK_F3, 0xFFFF }, { 0x04 } },                           // F3
    { { 0, kVK_F4, 0xFFFF }, { 0x0c } },                           // F4
    { { 0, kVK_F5, 0xFFFF }, { 0x03 } },                           // F5
    { { 0, kVK_F6, 0xFFFF }, { 0x0b } },                           // F6
    { { 0, kVK_F7, 0xFFFF }, { 0x83 } },                           // F7
    { { 0, kVK_F8, 0xFFFF }, { 0x0a } },                           // F8
    { { 0, kVK_F9, 0xFFFF }, { 0x01 } },                           // F9
    { { 0, kVK_F10, 0xFFFF }, { 0x09 } },                          // F10
    { { 0, kVK_F11, 0xFFFF }, { 0x78 } },                          // F11
    { { 0, kVK_F12, 0xFFFF }, { 0x07 } },                          // F12

    { { 0, kVK_F13, 0xFFFF }, { 0xe0, 0x7c } },                    // Print Screen/SysRq
    { { 0, kVK_F14, 0xFFFF }, { 0x7e } },                          // Scroll Lock
    { { 0, kVK_F15, 0xFFFF }, { 0xe1, 0x14, 0x77, 0xe1, 0xf0, 0x14, 0xf0, 0x77 } },    // Break

    { { 0, kVK_ANSI_KeypadClear, 0xFFFF }, { 0x77 } },             // Keypad Num Lock
    { { 0, kVK_ANSI_KeypadDivide, 0xFFFF }, { 0xe0, 0x4a } },      // Keypad /
    { { 0, kVK_ANSI_KeypadMultiply, 0xFFFF }, { 0x7c } },          // Keypad *
    { { 0, kVK_ANSI_Keypad7, 0xFFFF }, { 0x6c } },                 // Keypad 7
    { { 0, kVK_ANSI_Keypad8, 0xFFFF }, { 0x75 } },                 // Keypad 8
    { { 0, kVK_ANSI_Keypad9, 0xFFFF }, { 0x7d } },                 // Keypad 9
    { { 0, kVK_ANSI_KeypadMinus, 0xFFFF }, { 0x7b } },             // Keypad -
    { { 0, kVK_ANSI_Keypad4, 0xFFFF }, { 0x6b } },                 // Keypad 4
    { { 0, kVK_ANSI_Keypad5, 0xFFFF }, { 0x73 } },                 // Keypad 5
    { { 0, kVK_ANSI_Keypad6, 0xFFFF }, { 0x74 } },                 // Keypad 6
    { { 0, kVK_ANSI_KeypadPlus, 0xFFFF }, { 0x79 } },              // Keypad +
    { { 0, kVK_ANSI_Keypad1, 0xFFFF }, { 0x69 } },                 // Keypad 1
    { { 0, kVK_ANSI_Keypad2, 0xFFFF }, { 0x72 } },                 // Keypad 2
    { { 0, kVK_ANSI_Keypad3, 0xFFFF }, { 0x7a } },                 // Keypad 3
    { { 0, kVK_ANSI_Keypad0, 0xFFFF }, { 0x70 } },                 // Keypad 0
    { { 0, kVK_ANSI_KeypadDecimal, 0xFFFF }, { 0x71 } },           // Keypad .
    { { 0, kVK_ANSI_KeypadEnter, 0xFFFF }, { 0xe0, 0x5a } },       // Keypad Enter

    { { 0, kVK_Function, 0xFFFF }, { 0xe0, 0x70 } },               // Insert
    { { 0, kVK_ForwardDelete, 0xFFFF }, { 0xe0, 0x71 } },          // Delete
    { { 0, kVK_Home, 0xFFFF }, { 0xe0, 0x6c } },                   // Home
    { { 0, kVK_End, 0xFFFF }, { 0xe0, 0x69 } },                    // End
    { { 0, kVK_UpArrow, 0xFFFF }, { 0xe0, 0x75 } },                // Up
    { { 0, kVK_DownArrow, 0xFFFF }, { 0xe0, 0x72 } },              // Down
    { { 0, kVK_LeftArrow, 0xFFFF }, { 0xe0, 0x6b } },              // Left
    { { 0, kVK_RightArrow, 0xFFFF }, { 0xe0, 0x74 } },             // Right
    { { 0, kVK_PageUp, 0xFFFF }, { 0xe0, 0x7d } },                 // Page Up
    { { 0, kVK_PageDown, 0xFFFF }, { 0xe0, 0x7a } },               // Page Down

    { { 0, kVK_F16, 0xFFFF }, { 0xe0, 0x2f } },                    // Application (Win Menu)

    { { 0xFFFF }, { 0, 0 } },
};

typedef enum
{
    modifierKeyStateShift = 0,
    modifierKeyStateControl = 1,
    modifierKeyStateAlt = 2,
    modifierKeyStateCapsLock = 3,
    modifierKeyStateCommand = 4
} ModifierKeyCode;

typedef struct
{
    int keyState[5];
} ModifierState;

ModifierState modifierState;

typedef struct {
    uint32_t modifierMask;
    int checkMask;
    uint maskLeft;
    uint maskRight;
    uint8_t set_2_left[8];
    uint8_t set_2_right[8];
    int simulateMenuButton;
} ModifierMapInfo;

// The following are from the "NSEventModifierFlagOption" enumeration.
typedef enum
{
    nativeModifierFlagShift = (1 << 17),
    nativeModifierFlagControl = (1<< 18),
    nativeModifierFlagOption = (1 << 19),
    nativeModifierFlagCommand = (1 << 20)
} NativeModifierFlag;

static const ModifierMapInfo modifier_map[] = {
    {nativeModifierFlagShift, modifierKeyStateShift, 0x102, 0x104, {0x12}, {0x59}, 0 },                      // Shift.
    {nativeModifierFlagControl, modifierKeyStateControl, 0x101, 0x2100, {0x14}, {0xe0, 0x14}, 0},            // Control.
    {nativeModifierFlagOption, modifierKeyStateAlt, 0x120, 0x140, {0x11}, {0xe0, 0x11}, 0},                  // Alt.
    {nativeModifierFlagCommand, modifierKeyStateCommand, 0x100108, 0x100110, {0xe0, 0x1f}, {0xe0, 0x27}, 1}, // Command.
    {0x1<<31, 0, 0, 0, {0}, {0}, 0 },
};

int get_virtual_key_index(size_t k)
{
    if (key_map[k].virtual_key[0] == 0) return 1;
    
    for (int i = 1; i < MAX_KEYBOARD_LAYOUTS; i++)
    {
        if (key_map[k].virtual_key[i] == 0xFFFF) break;
        if (i == keyboardType) return i;
    }

    return 0;
}

const uint8_t *
keyboard_map_key(uint32_t native_scancode)
{
    size_t k;
    int index;
    
    for (k = 0; key_map[k].virtual_key[0] != 0xFFFF; k++) {
        index = get_virtual_key_index(k);

        if (key_map[k].virtual_key[index] == native_scancode) {
            return key_map[k].set_2;
        }
    }
    return NULL;
}

void keyboard_handle_modifier_keys(uint mask)
{
    size_t k;
    
    for (k = 0; modifier_map[k].modifierMask != (1U << 31); k++)
    {
        int state = modifierState.keyState[modifier_map[k].checkMask];
        uint modifierMask = modifier_map[k].modifierMask;
      
        if ((mask & modifierMask) != 0)
        {
            if (modifier_map[k].simulateMenuButton && config.mousetwobutton && state == 0)
            {
                state = 3;
                mouse_mouse_press(4);
            }
            else
            {
                if ((mask & modifier_map[k].maskLeft) == modifier_map[k].maskLeft && (state & 1) == 0)
                {
                    state |= 1;
                    keyboard_key_press(modifier_map[k].set_2_left);
                }

                if ((mask & modifier_map[k].maskRight) == modifier_map[k].maskRight && (state & 2) == 0)
                {
                    state |= 2;
                    keyboard_key_press(modifier_map[k].set_2_right);
                }
            }
        }
        else if ((mask & modifierMask) == 0 && state != 0)
        {
            if (config.mousetwobutton && modifier_map[k].simulateMenuButton)
            {
                state = 0;
                mouse_mouse_release(4);
            }
            else
            {
                if (state & 1)
                {
                    state &= ~1;
                    keyboard_key_release(modifier_map[k].set_2_left);
                }
                if (state & 2)
                {
                    state &= ~2;
                    keyboard_key_release(modifier_map[k].set_2_right);
                }
            }
        }

        modifierState.keyState[modifier_map[k].checkMask] = state;
    }
}

void keyboard_reset_modifiers(int sendReleaseEvent)
{
    size_t k;

    for (k = 0; modifier_map[k].modifierMask != (1U << 31); k++)
    {
        int state = modifierState.keyState[modifier_map[k].checkMask];
        
        if (sendReleaseEvent)
        {
            if (state & 1)
            {
                keyboard_key_release(modifier_map[k].set_2_left);
            }
            if (state & 2)
            {
                keyboard_key_release(modifier_map[k].set_2_right);
            }
        }
        
        modifierState.keyState[modifier_map[k].checkMask] = 0;
    }
}

void keyboard_configure_layout(const char *layoutName)
{
    if (!strcmp(layoutName, "British")) keyboardType = keyboardLayoutBritish;
    else if (!strcasecmp(layoutName, "French")) keyboardType = keyboardLayoutFrench;
    else keyboardType = keyboardLayoutUndefined;
    
    if (keyboardType == keyboardLayoutUndefined)
    {
        fprintf(stderr, "Unsupported keyboard layout '%s' - reverting to 'British' (0).\n", layoutName);
        keyboardType = keyboardLayoutBritish;
    }
    else
    {
        fprintf(stderr, "Using keyboard layout '%s' (%d).\n", layoutName, keyboardType);
    }
}

int keyboard_check_special_keys()
{
    return (modifierState.keyState[modifierKeyStateControl] != 0 && modifierState.keyState[modifierKeyStateCommand] != 0);
}
