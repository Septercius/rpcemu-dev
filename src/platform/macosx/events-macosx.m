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

#define UNUSED(x) (void)(x)

#include <stddef.h>
#include <stdint.h>

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDLib.h>

#include "rpcemu.h"

#include "events-macosx.h"


NativeEvent* handle_native_event(void *message)
{
    // This extracts information from the Cocoa event and passes it back up the chain to C++.
    NSEvent *event = (NSEvent *) message;
    
    NativeEvent *result = (NativeEvent *) malloc(sizeof(NativeEvent));
    
    // Only handle flags changed events, which are raised for modifier key changes.
    if (event.type == NSEventTypeFlagsChanged)
    {
        result->eventType = nativeEventTypeModifiersChanged;
        result->modifierMask = event.modifierFlags;
        result->processed = 1;
    }
    else
    {
        result->processed = 0;
    }
    
    // Return zero if the event is not handled here.
    return result;
}
