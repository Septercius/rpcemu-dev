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

#include <string.h>

#include "rpcemu.h"

/* These are functions that can be overridden by platforms if need
   be, but currently this version is used by Linux, all the other autoconf
   based builds and Windows. Only Mac OS X GUI version needs to override */

#ifdef __APPLE__
#include <dirent.h>

static char datadir[512] = "";

int rpcemu_set_datadir(const char *path)
{
  int len = strlen(path);
  if (len == 0) return 0;

  if (path[len - 1] != '/')
  {
    snprintf(datadir, 512, "%s/", path);
  }
  else
  {
    strncpy(datadir, path, 512);
  }

  DIR *ptr = opendir(datadir);
  if (ptr)
  {
    closedir(ptr);
    return 1;
  }

  return 0;
}

#else
static char datadir[512] = "./";
#endif

static char logpath[1024] = "";

/**
 * Return the path of the data directory containing all the sub data parts
 * used by the program, eg romload, hostfs etc.
 *
 * @return Pointer to static zero-terminated string of path
 */
const char *
rpcemu_get_datadir(void)
{
	return datadir;
}

/**
 * Return the full path to the RPCEmu log file.
 *
 * @return Pointer to static zero-terminated string of full path to log file
 */
const char *
rpcemu_get_log_path(void)
{
	if (logpath[0] == '\0') {
		strcpy(logpath, rpcemu_get_datadir());
		strcat(logpath, "rpclog.txt");
	}

	return logpath;
}
