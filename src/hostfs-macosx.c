#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <utime.h>
#include <sys/stat.h>

#include "hostfs_internal.h"
#include "paths.h"
#include "rpcemu.h"

#define UNUSED(x) (void)(x)

/**
 * Convert ADFS time-stamped Load-Exec addresses to the equivalent time_t.
 *
 * @param load RISC OS load address (assumed to be time-stamped)
 * @param exec RISC OS exec address (assumed to be time-stamped)
 * @return Time converted to time_t format
 *
 * Code adapted from fs/adfs/inode.c from Linux licensed under GPL2.
 * Copyright (C) 1997-1999 Russell King
 */
static time_t
hostfs_adfs2host_time(uint32_t load, uint32_t exec)
{
    uint32_t high = load << 24;
    uint32_t low  = exec;

    high |= low >> 8;
    low &= 0xff;

    if (high < 0x3363996a) {
        /* Too early */
        return 0;
    } else if (high >= 0x656e9969) {
        /* Too late */
        return 0x7ffffffd;
    }

    high -= 0x336e996a;
    return (((high % 100) << 8) + low) / 100 + (high / 100 << 8);
}

/**
 * Read information about an object.
 *
 * @param host_pathname Full Host path to object
 * @param object_info   Return object info (filled-in)
 */
void
hostfs_read_object_info_platform(const char *host_pathname,
                                 risc_os_object_info *object_info)
{
    struct stat info;
    uint32_t low, high;

    assert(host_pathname != NULL);
    assert(object_info != NULL);
    
    if (stat(host_pathname, &info)) {
        /* Error reading info about the object */
        switch (errno) {
        case ENOENT: /* Object not found */
        case ENOTDIR: /* A path component is not a directory */
            object_info->type = OBJECT_TYPE_NOT_FOUND;
            break;

        default:
            /* Other error */
            fprintf(stderr,
                    "hostfs_read_object_info_platform() could not stat() \'%s\': %s %d\n",
                    host_pathname, strerror(errno), errno);
            object_info->type = OBJECT_TYPE_NOT_FOUND;
            break;
        }

        return;
    }

    /* We were able to read about the object */
    if (S_ISREG(info.st_mode)) {
        object_info->type = OBJECT_TYPE_FILE;
    } else if (S_ISDIR(info.st_mode)) {
        object_info->type = OBJECT_TYPE_DIRECTORY;
    } else {
        /* Treat types other than file or directory as not found */
        object_info->type = OBJECT_TYPE_NOT_FOUND;
        return;
    }
    
    if (hostfs_is_systemfile_platform(host_pathname))
    {
        // A system file.  Should it be shown?
        if (!config.show_systemfiles)
        {
            object_info->type = OBJECT_TYPE_NOT_FOUND;
            return;
        }
    }
    else if (hostfs_is_dotfile_platform(host_pathname))
    {
        // A dot file.  Should it be shown?
        if (!config.show_dotfiles)
        {
            object_info->type = OBJECT_TYPE_NOT_FOUND;
            return;
        }
    }

    low  = (uint32_t) ((info.st_mtime & 255) * 100);
    high = (uint32_t) ((info.st_mtime / 256) * 100 + (low >> 8) + 0x336e996a);

    /* If the file has filetype and timestamp, additional values will need to be filled in later */
    object_info->load = (high >> 24);
    object_info->exec = (low & 0xff) | (high << 8);

    object_info->length = (uint32_t) info.st_size;
}

/**
 * Apply the timestamp to the supplied host object
 *
 * @param host_path Full path to object (file or dir) in host format
 * @param load      RISC OS load address (must contain time-stamp)
 * @param exec      RISC OS exec address (must contain time-stamp)
 */
void
hostfs_object_set_timestamp_platform(const char *host_path, uint32_t load, uint32_t exec)
{
    struct utimbuf t;

    t.actime = t.modtime = hostfs_adfs2host_time(load, exec);
    utime(host_path, &t);
    /* TODO handle error in utime() */
}

/**
 * Determines whether a file is a dot file.
 *
 * @param hostPath    Full path to object (file or dir) in host format
 *
 * @return            1 if the file is a dot file, otherwise 0.
 */
int
hostfs_is_dotfile_platform(const char *hostPath)
{
    const char *fileName = path_extract_filename(hostPath);
    if (fileName[0] == '.') return 1;
    
    return 0;
}

/**
 * Determines whether a file is a system file.
 *
 * @param hostPath    Full path to object (file or dir) in host format
 *
 * @return            1 if the file is a system file, otherwise 0.
 */
int
hostfs_is_systemfile_platform(const char *hostPath)
{
    const char *leafNames[] = { ".DS_Store", ".fseventsd", "System Volume Information", ".Spotlight-V100", ".Trash" };
    const char *fullPaths[] = { "/bin", "/cores", "/dev", "/etc", "/home", "/net", "/private", "/sbin", "/tmp", "/usr", "/var", "/installer.failurerequests", "/Network", "/Volumes" };
    
    const char *fileName = path_extract_filename(hostPath);
    size_t i;
    
    // Check file names.
    for (i = 0; i < sizeof(leafNames) / sizeof(leafNames[0]); i += 1)
    {
        if (!strcasecmp(fileName, leafNames[i])) return 1;
    }
    
    // Check full paths.
    for (i = 0; i < sizeof(fullPaths) / sizeof(fullPaths[0]); i += 1)
    {
        if (!strcasecmp(hostPath, fullPaths[i])) return 1;
    }
    
    return 0;
}

