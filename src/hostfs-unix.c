#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <utime.h>
#include <sys/stat.h>

#include "hostfs_internal.h"

// Use 64-bit struct timespec for time conversion?
// Currently enabled on 64-bit Linux.
#if defined(__LP64__) && defined(__linux__)

/**
 * Convert a 64-bit struct timespec to the equivalent RISC OS time.
 *
 * A 64-bit struct timespec has greater range and precision, so these may be
 * lost in the conversion.
 *
 * @param      t    Pointer to 64-bit struct timespec
 * @param[out] load Pointer to uint32_t, set to high 8 bits of RISC OS time
 * @param[out] exec Pointer to uint32_t, set to low 32 bits of RISC OS time
 */
static void
hostfs_timespec64_to_risc_os_time(const struct timespec *t, uint32_t *load, uint32_t *exec)
{
	int64_t centiseconds;
	uint64_t ro_time;

	if (t->tv_sec < INT64_C(-2208988800)) {
		// Too early
		*load = 0;
		*exec = 0;
		return;
	}

	if (t->tv_sec > INT64_C(8786127477) ||
	    ((t->tv_sec == INT64_C(8786127477) && t->tv_nsec >= 750000000)))
	{
		// Too late
		// A return value of the latest time is interpreted as a
		// load-exec pair, so return one less than the max
		*load = 0xff;
		*exec = 0xfffffffe;
		return;
	}

	centiseconds = (t->tv_sec * 100) + (t->tv_nsec / 10000000);

	ro_time = (uint64_t) (centiseconds + INT64_C(0x336e996a00));

	assert(ro_time <= UINT64_C(0xffffffffff));

	*load = (uint32_t) (ro_time >> 32);
	*exec = (uint32_t) ro_time;
}

/**
 * Convert ADFS time-stamped Load-Exec addresses to the equivalent 64-bit
 * struct timespec.
 *
 * @param load RISC OS load address (must contain time-stamp)
 * @param exec RISC OS exec address (must contain time-stamp)
 * @return Equivalent 64-bit struct timespec of given RISC OS time
 */
static struct timespec
hostfs_risc_os_time_to_timespec64(uint32_t load, uint32_t exec)
{
	const uint64_t centiseconds = ((uint64_t) (load & 0xff) << 32) | (uint64_t) exec;
	const uint64_t seconds = centiseconds / 100;
	struct timespec result;

	result.tv_sec = (int64_t) (seconds - UINT64_C(2208988800));
	result.tv_nsec = (int64_t) (centiseconds - (seconds * 100)) * 10000000;

	return result;
}

/**
 * Extract the modification time from a 'struct stat'.
 * (Specifically the st_mtim field which is a 'struct timespec').
 *
 * Convert this time to an equivalent RISC OS time, and store in the
 * Load-Exec addresses of a 'risc_os_object_info'.
 *
 * @param      s           Pointer to a 'struct stat'
 * @param[out] object_info Pointer to object info in which Load-Exec are filled in
 */
static void
hostfs_struct_stat_to_risc_os_time(const struct stat *s, risc_os_object_info *object_info)
{
	uint32_t load, exec;

	hostfs_timespec64_to_risc_os_time(&s->st_mtim, &load, &exec);
	object_info->load = load;
	object_info->exec = exec;
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
	struct timespec t[2];

	t[0] = hostfs_risc_os_time_to_timespec64(load, exec);
	t[1] = t[0];

	utimensat(AT_FDCWD, host_path, t, 0);
	// TODO handle error in utimensat()
}

#else // Not using 64-bit struct timespec for time conversion

/**
 * Convert a time_t to the equivalent RISC OS time.
 *
 * @param      t    Time as time_t
 * @param[out] load Pointer to uint32_t, set to high 8 bits of RISC OS time
 * @param[out] exec Pointer to uint32_t, set to low 32 bits of RISC OS time
 *
 * Code adapted from fs/adfs/inode.c from Linux licensed under GPL2.
 * Copyright (C) 1997-1999 Russell King
 */
static void
hostfs_time_t_to_risc_os_time(time_t t, uint32_t *load, uint32_t *exec)
{
	uint32_t low, high;

	low  = (uint32_t) ((t & 255) * 100);
	high = (uint32_t) ((t / 256) * 100 + (low >> 8) + 0x336e996a);

	*load = (high >> 24);
	*exec = (low & 0xff) | (high << 8);
}

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
 * Extract the modification time from a 'struct stat'.
 * (Specifically the st_mtime field which is a time_t).
 *
 * Convert this time to an equivalent RISC OS time, and store in the
 * Load-Exec addresses of a 'risc_os_object_info'.
 *
 * @param      s           Pointer to a 'struct stat'
 * @param[out] object_info Pointer to object info in which Load-Exec are filled in
 */
static void
hostfs_struct_stat_to_risc_os_time(const struct stat *s, risc_os_object_info *object_info)
{
	uint32_t load, exec;

	hostfs_time_t_to_risc_os_time(s->st_mtime, &load, &exec);
	object_info->load = load;
	object_info->exec = exec;
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
	// TODO handle error in utime()
}

#endif

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

	/* If the file has filetype and timestamp, additional values will need to be filled in later */
	hostfs_struct_stat_to_risc_os_time(&info, object_info);

	object_info->length = info.st_size;
}
