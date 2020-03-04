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

#ifndef __PREFERENCES_MACOSX_H__
#define __PREFERENCES_MACOSX_H__

#ifdef __cplusplus
extern "C" {
#endif

extern void init_preferences(void);
extern void preferences_set_data_directory(const char *path);
extern const char *preferences_get_data_directory();

extern bool promptForDataDirectory;
  
#ifdef __cplusplus
}
#endif

#endif // __PREFERENCES_MACOSX_H__
