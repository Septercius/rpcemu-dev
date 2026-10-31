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
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>

#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "choose-dialog.h"
#include "mem.h"
#include "paths.h"
#include "platform-common.h"
#include "preferences-macosx.h"
#include "rpcemu.h"
#include "sound.h"
#include "vidc20.h"

static char datadir[PATH_MAX];
static char logpath[PATH_MAX];

int rpcemu_set_datadir(const char *path);

bool rpcemu_platform_startup(int modifierMask)
{
    char folderPath[PATH_MAX];
    
    // Look for the location of the data folder in the macOS preferences.
    // If this cannot be found, or the user holds down the SHIFT key as the application starts, 
    // they will be prompted to select a folder.
    bool initialised = preferences_init(folderPath);
    if (!initialised || (modifierMask && StartupModifierKey_Shift) != 0)
    {
        // Prompt the user to select a folder for storing the data.
        ChooseDialog dialog;
        if (dialog.exec() == QDialog::Accepted)
        {
            const char *path = preferences_get_data_directory();
            
            if (!rpcemu_set_datadir(path))
            {
                return false;
            }
        }
        else
        {
            // If the user cancelled, do not continue.
            return false;
        }
    }
    else
    {
        rpcemu_set_datadir(folderPath);
    }

    rpcemu_log_information();
    config_load(&config);
    
    return true;
}

/**
 * Return disk space information about a file system.
 *
 * @param path Pathname of object within file system
 * @param d    Pointer to disk_info structure that will be filled in
 * @return     On success 1 is returned, on error 0 is returned
 */
int path_disk_info(const char *path, disk_info *d)
{
    struct statvfs s;
    int ret;

    assert(path != NULL);
    assert(d != NULL);

    if ((ret = statvfs(path, &s)) != 0)
    {
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
void rpcemu_log_os(void)
{
    struct utsname u;

    if (uname(&u) == -1)
    {
        rpclog("OS: Could not determine: %s\n", strerror(errno));
        return;
    }

    rpclog("OS: SysName = %s\n", u.sysname);
    rpclog("OS: Release = %s\n", u.release);
    rpclog("OS: Version = %s\n", u.version);
    rpclog("OS: Machine = %s\n", u.machine);
}

/**
 * Return the path of the data directory containing all the sub data parts
 * used by the program, eg romload, hostfs etc.
 *
 * @return Pointer to static zero-terminated string of path
 */
const char *rpcemu_get_datadir(void)
{
    return datadir;
}

/**
 * Return the full path to the RPCEmu log file.
 *
 * @return Pointer to static zero-terminated string of full path to log file
 */
const char *rpcemu_get_log_path(void)
{
    if (logpath[0] == '\0')
    {
        path_append_data(logpath, "rpclog.txt");
    }

    return logpath;
}

/**
 * Set the directory that contains the data required by
 * the program, e.g. CMOS settings, ROMs, configuration
 *
 * @return 1 if successful, otherwise 0.
 */
int rpcemu_set_datadir(const char *path)
{
    size_t len = strlen(path);
    if (len == 0 || !path_directory_exists(path))
    {
        return 0;
    }
    
    strncpy(datadir, path, PATH_MAX);
    
    return 1;
}

