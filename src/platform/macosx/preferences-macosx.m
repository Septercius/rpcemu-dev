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

#define UNUSED(x) (void) (x)

#include <dirent.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <Carbon/Carbon.h>
#include <Cocoa/Cocoa.h>
#include <IOKit/hid/IOHIDLib.h>

#include "paths.h"
#include "platform-macosx.h"
#include "rpcemu.h"

NSString *const KeyDataDirectory = @"DataDirectory";

bool preferences_init(char *folderPath)
{
    NSMutableDictionary *defaultValues = [NSMutableDictionary dictionary];
    [defaultValues setObject:@"" forKey:KeyDataDirectory];

    [[NSUserDefaults standardUserDefaults] registerDefaults:defaultValues];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    // Check to see if there is a proper path for the data directory.
    // If not, prompt for one.
    NSString *dataDirectory = [defaults stringForKey:KeyDataDirectory];
    if (dataDirectory == nil || [dataDirectory length] == 0)
    {
        return false;
    }
    else
    {
        const char *str = [dataDirectory UTF8String];
        
        // Check the folder exists.
        if (path_directory_exists(str))
        {
            strcpy(folderPath, str);
            return true;
        }
        
        folderPath = NULL;
        return false;
    }
}

void preferences_set_data_directory(const char *path)
{
    NSString *dataDirectory = [NSString stringWithUTF8String:path];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:dataDirectory forKey:KeyDataDirectory];
}

const char *preferences_get_data_directory()
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *path = [defaults stringForKey:KeyDataDirectory];

    return [path UTF8String];
}
