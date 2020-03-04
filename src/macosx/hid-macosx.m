///*
// RPCEmu - An Acorn system emulator
//
// Copyright (C) 2017 Matthew Howkins
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
// */

#include <stddef.h>
#include <stdint.h>

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDLib.h>

#include "keyboard.h"
#include "keyboard_macosx.h"

#define UNUSED(x) (void)(x)

static IOHIDManagerRef hidManager = NULL;

CFDictionaryRef createHIDDeviceMatchingDictionary(uint32 usagePage, uint32 usage)
{
    CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (dictionary)
    {
        CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usagePage);
        if (number)
        {
            CFDictionarySetValue(dictionary, CFSTR(kIOHIDDeviceUsagePageKey), number);
            CFRelease(number);

            number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
            if (number)
            {
                CFDictionarySetValue(dictionary, CFSTR(kIOHIDDeviceUsageKey), number);
                CFRelease(number);

                return dictionary;
            }
        }

        CFRelease(dictionary);
    }

    return NULL;
}

void processHIDCallback(void *context, IOReturn result, void *sender, IOHIDValueRef value)
{
    UNUSED(result);
    UNUSED(sender);

    if (context != hidManager) return;

    IOHIDElementRef element = IOHIDValueGetElement(value);
    if (IOHIDElementGetUsagePage(element) != kHIDPage_KeyboardOrKeypad || IOHIDElementGetUsage(element) != kHIDUsage_KeyboardCapsLock) return;

    CFIndex pressed = IOHIDValueGetIntegerValue(value);

    uint8 scanCodes[] = { 0x58 };

    if (pressed == 0)
    {
        keyboard_key_release(scanCodes);
    }
    else
    {
        keyboard_key_press(scanCodes);
    }
}

const char *getCurrentKeyboardLayoutName()
{
    TISInputSourceRef currentSource = TISCopyCurrentKeyboardInputSource();
    NSString *inputSource = (__bridge NSString *)(TISGetInputSourceProperty(currentSource, kTISPropertyInputSourceID));
    NSUInteger lastIndex = [inputSource rangeOfString:@"." options:NSBackwardsSearch].location;
    
    NSString *layoutName = [inputSource substringFromIndex: lastIndex + 1];
    lastIndex = [layoutName rangeOfString:@" - "].location;
    
    if (lastIndex != NSNotFound) layoutName = [layoutName substringToIndex: lastIndex];
   
    return [layoutName UTF8String];
}

void terminate_hid_manager(void)
{
    if (!hidManager) return;
    
    IOHIDManagerUnscheduleFromRunLoop(hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerRegisterInputValueCallback(hidManager, NULL, NULL);
    IOHIDManagerClose(hidManager, 0);
    
    CFRelease(hidManager);
    
    hidManager = NULL;
}

void init_hid_manager(void)
{
    const char *layoutName = getCurrentKeyboardLayoutName();
    keyboard_configure_layout(layoutName);

    hidManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!hidManager) return;

    CFDictionaryRef keyboard = NULL, keypad = NULL;
    CFArrayRef matches = NULL;

    keyboard = createHIDDeviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard);
    if (!keyboard)
    {
        IOHIDManagerClose(hidManager, 0);
        return;
    }

    keypad = createHIDDeviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Keypad);
    if (!keypad)
    {
        CFRelease(keyboard);
        IOHIDManagerClose(hidManager, 0);

        return;
    }

    CFDictionaryRef matchesList[] = {keyboard, keypad};
    matches = CFArrayCreate(kCFAllocatorDefault, (const void**) matchesList, 2, NULL);
    if (!matches)
    {
        CFRelease(keypad);
        CFRelease(keyboard);
        IOHIDManagerClose(hidManager, 0);

        return;
    }

    IOHIDManagerSetDeviceMatchingMultiple(hidManager, matches);
    IOHIDManagerRegisterInputValueCallback(hidManager, processHIDCallback, hidManager);
    IOHIDManagerScheduleWithRunLoop(hidManager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    if (IOHIDManagerOpen(hidManager, kIOHIDOptionsTypeNone) != kIOReturnSuccess)
    {
        terminate_hid_manager();
    }

    CFRelease(matches);
    CFRelease(keypad);
    CFRelease(keyboard);
}


