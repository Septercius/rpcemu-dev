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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "rpcemu.h"
#include "podules.h"
#include "podulerom.h"

#define MAXROMS 16
static char romfns[MAXROMS + 1][256];

static uint8_t *podulerom = NULL;
static uint32_t poduleromsize = 0;
static uint32_t chunkbase;
static uint32_t filebase;

static const char description[] = "RPCEmu Support";

/**
 *
 *
 * @param type
 * @param filebase
 * @param size
 */
static void
makechunk(uint8_t type, uint32_t filebase, uint32_t size)
{
	podulerom[chunkbase++] = type;
	podulerom[chunkbase++] = (uint8_t) size;
	podulerom[chunkbase++] = (uint8_t) (size >> 8);
	podulerom[chunkbase++] = (uint8_t) (size >> 16);

	podulerom[chunkbase++] = (uint8_t) filebase;
	podulerom[chunkbase++] = (uint8_t) (filebase >> 8);
	podulerom[chunkbase++] = (uint8_t) (filebase >> 16);
	podulerom[chunkbase++] = (uint8_t) (filebase >> 24);
}

/**
 * Podule byte read function for podulerom
 *
 * @param p    podule pointer (unused)
 * @param easi Read from EASI space or from regular IO space
 * @param addr Address of byte to read
 * @return Contents of byte
 */
static uint8_t
podulerom_read8(podule *p, int easi, uint32_t addr)
{
	NOT_USED(p);

	if (easi && (poduleromsize > 0)) {
		addr = (addr & 0x00ffffff) >> 2;
		if (addr < poduleromsize) {
			return podulerom[addr];
		}
		return 0x00;
	}
	return 0xff;
}

/**
 * Add the ROM Podule to the list of active podules.
 *
 * Called on emulated machine reset
 */
void
podulerom_reset(void)
{
	addpodule(NULL, NULL, NULL, NULL, NULL, podulerom_read8, NULL, NULL, 0);
}

/**
 * Initialise the ROM Podule by loading files and building a ROM image
 * dynamically.
 *
 * Called on program startup
 */
void
initpodulerom(void)
{
	int file = 0;
	int i;
	char romdirectory[512];
	DIR *dir;
	const struct dirent *d;

	/* Build podulerom directory path */
	snprintf(romdirectory, sizeof(romdirectory), "%spoduleroms/", rpcemu_get_datadir());

	if (podulerom != NULL) {
		free(podulerom);
	}
	poduleromsize = 0;

	/* Scan directory for podule files */
	dir = opendir(romdirectory);
	if (dir != NULL) {
		while ((d = readdir(dir)) != NULL && file < MAXROMS) {
			const char *ext = rpcemu_file_get_extension(d->d_name);
			char filepath[512];
			struct stat buf;

			snprintf(filepath, sizeof(filepath), "%s%s", romdirectory, d->d_name);

			if (stat(filepath, &buf) == 0) {
				/* Skip directories or files with a .txt extension or starting with '.' */
				if (S_ISREG(buf.st_mode) && (strcasecmp(ext, "txt") != 0) && d->d_name[0] != '.') {
					strcpy(romfns[file++], d->d_name);
				}
			}
		}
		closedir(dir);
	} else {
		rpclog("Could not open podulerom directory '%s': %s\n",
		      romdirectory, strerror(errno));
	}

	/* Build podulerom header */
	chunkbase = 0x10;
	filebase = chunkbase + 8 * file + 8;
	poduleromsize = filebase + ((sizeof(description) + 3) & ~3u); /* Word align description string */
	podulerom = malloc(poduleromsize);
	if (podulerom == NULL) {
		fatal("initpodulerom: Out of Memory");
	}

	memset(podulerom, 0, poduleromsize);
	podulerom[0] = 0; /* Acorn comformant card, not requesting FIQ, not requesting interupt, EcID = 0 = EcID is extended (8 bytes) */
	podulerom[1] = 3; /* Interrupt status has been relocated, chunk directories present, byte access */
	podulerom[2] = 0; /* Mandatory */
	podulerom[3] = 0; /* Product type, low,  ???? */
	podulerom[4] = 0; /* Product type, high, ???? */
	podulerom[5] = 0; /* Manufacturer, low,  Acorn UK */
	podulerom[6] = 0; /* Manufacturer, high, Acorn UK */
	podulerom[7] = 0; /* Reserved */

	memcpy(podulerom + filebase, description, sizeof(description));
	makechunk(0xf5, filebase, sizeof(description)); /* f = Device Data, 5 = description */
	filebase += (sizeof(description) + 3) & ~3u;

	/* Add each file into the podule's rom */
	for (i = 0; i < file; i++) {
		FILE *f;
		char filepath[512];
		long len;

		snprintf(filepath, sizeof(filepath), "%s%s", romdirectory, romfns[i]);

		f = fopen(filepath, "rb");
		if (f == NULL) {
			fatal("initpodulerom: Can't open podulerom file '%s': '%s'", romfns[i], strerror(errno));
		}
		fseek(f, 0, SEEK_END);
		len = ftell(f);
		if (len < 0) {
			fatal("initpodulerom: Error reading size of podule ROM file '%s': %s",
			      romfns[i], strerror(errno));
		}
		if (len > 4096 * 1024) {
			fatal("initpodulerom: Cannot have files larger than 4MB in podule ROM: '%s'", romfns[i]);
		}
		poduleromsize += ((uint32_t) len + 3) & ~3u;
		if (poduleromsize > 4096 * 1024) {
			fatal("initpodulerom: Cannot have more than 4MB of podule ROM files");
		}
		podulerom = realloc(podulerom, poduleromsize);
		if (podulerom == NULL) {
			fatal("initpodulerom: Out of Memory");
		}

		fseek(f, 0, SEEK_SET);
		if (fread(podulerom + filebase, 1, (size_t) len, f) != (size_t) len) {
			fatal("initpodulerom: Failed to read file '%s': %s",
			      romfns[i], strerror(errno));
		}
		fclose(f);
		rpclog("initpodulerom: Successfully loaded '%s' into podulerom\n",
		       romfns[i]);
		makechunk(0x81, filebase, len); /* 8 = Mandatory, Acorn Operating System #0 (RISC OS), 1 = BBC ROM */
		filebase += ((uint32_t) len + 3) & ~3u;
	}
}
