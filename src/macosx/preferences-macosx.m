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
#include <dirent.h>

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDLib.h>

#include "rpcemu.h"

bool promptForDataDirectory;
static NSString* const KeyDataDirectory = @"DataDirectory";

void init_preferences(void)
{
  NSMutableDictionary *defaultValues = [NSMutableDictionary dictionary];
  [defaultValues setObject: @"" forKey:KeyDataDirectory];
  
  [[NSUserDefaults standardUserDefaults] registerDefaults: defaultValues];
  
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  
  // Check to see if there is a proper path for the data directory.
  // If not, prompt for one.
  NSURL *dataDirectory = [defaults URLForKey: KeyDataDirectory];
  if (dataDirectory == nil || [dataDirectory checkResourceIsReachableAndReturnError:nil] == NO)
  {
    promptForDataDirectory = true;
  }
  else
  {
    const char *str = [dataDirectory fileSystemRepresentation];

    // Check the folder exists.
    DIR *ptr = opendir(str);
    if (ptr)
    {
      closedir(ptr);
      rpcemu_set_datadir(str);
      
      promptForDataDirectory = false;
    }
    else
    {
      promptForDataDirectory = true;
    }
  }
}

void preferences_set_data_directory(const char *path)
{
  @autoreleasepool {
  NSURL *dataDirectory = [NSURL fileURLWithFileSystemRepresentation:path 
  isDirectory:YES relativeToURL:nil];
  
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  [defaults setURL:dataDirectory forKey:KeyDataDirectory];
  }
}

const char* preferences_get_data_directory()
{
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSURL *path = [defaults URLForKey: KeyDataDirectory];
  
  return [path fileSystemRepresentation];
}
