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

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include <pthread.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "rpcemu.h"
#include "mem.h"
#include "sound.h"
#include "vidc20.h"




/**
 * Return disk space information about a file system.
 *
 * @param path Pathname of object within file system
 * @param d    Pointer to disk_info structure that will be filled in
 * @return     On success 1 is returned, on error 0 is returned
 */
int
path_disk_info(const char *path, disk_info *d)
{
    struct statvfs s;
    int ret;

    assert(path != NULL);
    assert(d != NULL);

    if ((ret = statvfs(path, &s)) != 0) {
        return 0;
    }

    d->size = (uint64_t) s.f_blocks * (uint64_t) s.f_frsize;
    d->free = (uint64_t) s.f_bavail * (uint64_t) s.f_frsize;

    return 1;
}

/**
 * Log details about the current Operating System version.
 *
 * This function should work on all Unix and Unix-like systems.
 *
 * Called during program start-up.
 */
void
rpcemu_log_os(void)
{
    struct utsname u;

    if (uname(&u) == -1) {
        rpclog("OS: Could not determine: %s\n", strerror(errno));
        return;
    }

    rpclog("OS: SysName = %s\n", u.sysname);
    rpclog("OS: Release = %s\n", u.release);
    rpclog("OS: Version = %s\n", u.version);
    rpclog("OS: Machine = %s\n", u.machine);
}
