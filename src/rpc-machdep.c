#include <allegro.h>

#include "rpcemu.h"

/* These are functions that can be overridden by platforms if need
   be, but currently this version is used by Linux, all the other autoconf
   based builds and Windows. Only Mac OS X GUI version needs to override */

static char datadir[512] = "";

/**
 * Return the path of the data directory containing all the sub data parts
 * used by the program, eg romload, hostfs etc.
 *
 * @return Pointer to static zero-terminated string of path
 */
const char *
rpcemu_get_datadir(void)
{
	if (datadir[0] == '\0') {
		char *p;

		get_executable_name(datadir, 511);
		p = get_filename(datadir);
		*p = '\0';
	}

	return datadir;
}
