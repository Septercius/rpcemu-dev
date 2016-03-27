/*RPCemu v0.6 by Tom Walker
  ROM loader*/
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <allegro.h>

#include "rpcemu.h"
#include "mem.h"
#include "romload.h"

#define MAXROMS 16 /**< Allow up to this many files for a romimage to be broken up into */

/* Website with help on finding romimages */
#define ROM_WEB_SITE "http://www.marutan.net/rpcemu"

#define ROM_WEB_SITE_STRING "For information on how to acquire ROM images please visit\n" ROM_WEB_SITE

typedef struct {
	uint32_t	addr_data;	///< Address to try matching data
	uint32_t	data[4];	///< Data that must match
	uint32_t	addr_replace;	///< Address of replacement data
	uint32_t	replace;	///< Replacement data
	const char	*comment;	///< Comment that will be added to logfile
} rom_patch_t;

static const rom_patch_t rom_patch[] = {
	// Patching for 8MB VRAM
	{ 0x138c0, { 0xe3a00402, 0xe2801004, 0xeb000128, 0x03a06002 }, 0x138cc, 0x03a06008, "8MB VRAM RISC OS 3.50" },
	{ 0x1411c, { 0xe3a00402, 0xe2801004, 0xeb000122, 0x03a06002 }, 0x14128, 0x03a06008, "8MB VRAM RISC OS 3.60" },
	{ 0x15874, { 0xe3a00402, 0xe2801004, 0xeb000143, 0x03a06002 }, 0x15880, 0x03a06008, "8MB VRAM RISC OS 3.70" },
	{ 0x15898, { 0xe3a00402, 0xe2801004, 0xeb000143, 0x03a06002 }, 0x158a4, 0x03a06008, "8MB VRAM RISC OS 3.71" },
	{ 0x14744, { 0xe3a00402, 0xe2801004, 0xeb000148, 0x03a06002 }, 0x14750, 0x03a06008, "8MB VRAM RISC OS 4.02" },
	{ 0x148e8, { 0xe3a00402, 0xe2801004, 0xeb0001ae, 0x03a06002 }, 0x148f4, 0x03a06008, "8MB VRAM RISC OS 4.04" },
	{ 0x14150, { 0xe3a00402, 0xe2801004, 0xeb0001ad, 0x03a06002 }, 0x1415c, 0x03a06008, "8MB VRAM RISC OS 4.29" },
	{ 0x1473c, { 0xe3a00402, 0xe2801004, 0xeb0001ad, 0x03a06002 }, 0x14748, 0x03a06008, "8MB VRAM RISC OS 4.33" },
	{ 0xe504,  { 0xe3a00402, 0xe2801004, 0xeb0001ad, 0x03a06002 }, 0xe510,  0x03a06008, "8MB VRAM RISC OS 4.37" },
	{ 0xe248,  { 0xe3a00402, 0xe2801004, 0xeb0001ae, 0x03a06002 }, 0xe254,  0x03a06008, "8MB VRAM RISC OS 4.39" },
	{ 0x8a764, { 0xe1a00001, 0xe2801004, 0xeb00000d, 0x03a06002 }, 0x8a770, 0x03a06008, "8MB VRAM RISC OS 6.02" },
};

/**
 * Scan through the table of ROM patches, looking for a match with the current
 * ROM. If a match is found, make the required change and log the patch name.
 */
static void
romload_patch(void)
{
	const rom_patch_t *p;
	int i;

	for (i = 0, p = rom_patch; i < sizeof(rom_patch) / sizeof(rom_patch[0]); i++, p++) {
		uint32_t addr = p->addr_data;
		const uint32_t *data = p->data;

		if (rom[addr >> 2] == data[0] &&
		    rom[(addr + 4) >> 2] == data[1] &&
		    rom[(addr + 8) >> 2] == data[2] &&
		    rom[(addr + 12) >> 2] == data[3])
		{
			// Patch the data
			rom[p->addr_replace >> 2] = p->replace;

			// Log the patch
			rpclog("romload: ROM patch applied: %s\n", p->comment);
		}
	}
}

/**
 * qsort comparison function for alphabetical sorting of
 *  C char *pointers. From the qsort() manpage
 *
 * @param p1 First item to compare
 * @param p2 Second item to compare
 * @return Integer less than, equal to, or greater than zero
 */
static int cmpstringp(const void *p1, const void *p2)
{
        /* The actual arguments to this function are "pointers to
           pointers to char", so assign to variables of this type.
           Then we dereference as we pass them to strcmp(). */

        const char * const *pstr1 = p1;
        const char * const *pstr2 = p2;

        return strcmp(*pstr1, *pstr2);
}

/**
 * Load the ROM images, calls fatal() on error.
 */
void loadroms(void)
{
        int number_of_files = 0;
        int c;
        int pos = 0;
        const char *dirname = "roms";
        char *romfilenames[MAXROMS];
	char romdirectory[512];
	DIR *dir;
	const struct dirent *d;

	/* Build rom directory path */
	snprintf(romdirectory, sizeof(romdirectory), "%s%s/", rpcemu_get_datadir(), dirname);

	/* Scan directory for ROM files */
	dir = opendir(romdirectory);
	if (dir != NULL) {
		while ((d = readdir(dir)) != NULL && number_of_files < MAXROMS) {
			const char *ext = get_extension(d->d_name);
			char filepath[512];
			struct stat buf;

			snprintf(filepath, sizeof(filepath), "%s%s", romdirectory, d->d_name);

			if (stat(filepath, &buf) == 0) {
				/* Skip directories or files with a .txt extension or starting with '.' */
				if (S_ISREG(buf.st_mode) && (strcasecmp(ext, "txt") != 0) && d->d_name[0] != '.') {
					romfilenames[number_of_files] = strdup(d->d_name);
					if (romfilenames[number_of_files] == NULL) {
						fatal("Out of memory in loadroms()");
					}
					number_of_files++;
				}
			}
		}
		closedir(dir);
	} else {
		fatal("Could not open ROM files directory '%s': %s\n",
		      romdirectory, strerror(errno));
	}

        /* Empty directory? or only .txt files? */
        if (number_of_files == 0) {
                fatal("Could not load ROM files from directory '%s'\n\n"
                      ROM_WEB_SITE_STRING "\n",
                      dirname);
        }

        /* Sort filenames into alphabetical order */
        qsort(romfilenames, number_of_files, sizeof(char *), cmpstringp);

        /* Load files */
        for (c = 0; c < number_of_files; c++) {
                FILE *f;
                int len;
                char filepath[512];

                snprintf(filepath, sizeof(filepath), "%s%s", romdirectory, romfilenames[c]);

                f = fopen(filepath, "rb");
                if (f == NULL) {
                        fatal("Can't open ROM file '%s': %s", filepath,
                              strerror(errno));
                }

                /* Calculate file size */
                fseek(f, 0, SEEK_END);
                len = ftell(f);

                if (pos + len > ROMSIZE) {
                        fatal("ROM files larger than 8MB");
                }

                /* Read file data */
                rewind(f);
                if (fread(&romb[pos], len, 1, f) != 1) {
                        fatal("Error reading from ROM file '%s': %s",
                              romfilenames[c], strerror(errno));
                }

                fclose(f);

		rpclog("romload: Loaded '%s' %d bytes\n", romfilenames[c], len);

                pos += len;

                /* Free up filename allocated earlier */
                free(romfilenames[c]);
        }

        /* Reject ROMs that are not sensible sizes
         * Allow 2MB (RISC OS 3.50)
         *       4MB (RISC OS 3.60 -> Half way through Select)
         *       6MB (Later Select)
         *       8MB (Current maximum)
         */
        if (pos != (2 * 1024 * 1024) && pos != (4 * 1024 * 1024)
            && pos != (6 * 1024 * 1024) && pos != (8 * 1024 * 1024))
        {
                fatal("ROM Image of unsupported size: expecting 2MB, 4MB, 6MB or 8MB, got %d bytes", pos);
        }

	rpclog("romload: Total ROM size %d MB\n", pos / 1048576);

#ifdef _RPCEMU_BIG_ENDIAN
	/* Endian swap */
	for (c = 0; c < pos; c += 4) {
		uint32_t temp = rom[c >> 2];

		rom[c >> 2] = (temp >> 24) |
		              ((temp >> 8) & 0x0000ff00) |
		              ((temp << 8) & 0x00ff0000) |
		              (temp << 24);
	}
#endif

	/* Patch ROM  */
	romload_patch();

	/* Patch Netstation versions of NCOS to bypass the results of the POST that we currently fail */
	/* NCOS 0.10 */
	if (rom[0x2714 >> 2] == 0xe1d70000) {
		rom[0x2714 >> 2] = 0xe3b00000; /* MOVS r0, #0 */
		rom[0x2794 >> 2] = 0xe3b00000; /* MOVS r0, #0 */
	}

	/* NCOS 1.06/1.11 */
	if (rom[0x26f0 >> 2] == 0xe1d70000) {
		rom[0x26f0 >> 2] = 0xe3b00000; /* MOVS r0, #0 */
		rom[0x2750 >> 2] = 0xe3b00000; /* MOVS r0, #0 */
	}
}
