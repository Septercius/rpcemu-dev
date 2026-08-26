/*
 RPCEmu - An Acorn system emulator
 
 Copyright (C) 2016-2017 Peter Howkins
 
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

#ifndef PATHS_H
#define PATHS_H

#ifdef __cplusplus
extern "C" {
#endif

extern const char *path_extract_filename(const char *path);
extern char *path_resolve(const char *path);
extern int path_validate(const char *path);
extern void path_join(const char *str1, const char *str2, char *buffer);
extern void path_join_to(char *str1, const char *str2);

#ifdef __cplusplus
}
#endif

#endif
