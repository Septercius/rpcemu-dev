/*RPCemu v0.6 by Tom Walker
  ROM loader*/
#include <stdint.h>
#include <stdlib.h>
#include <allegro.h>
#include <stdio.h>
#include "rpcemu.h"
#include "mem.h"
#include "romload.h"

#define MAXROMS 16

/* Website with help on finding romimages */
#define ROM_WEB_SITE "http://www.marutan.net/rpcemu"

#define ROM_WEB_SITE_STRING "For information on how to acquire ROM images please visit\n" ROM_WEB_SITE

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
        int finished;
        int number_of_files = 0;
        int c;
        int pos = 0;
        struct al_ffblk ff;
        char olddir[512],fn[512];
        char *ext;
        const char *wildcard = "*.*";
        const char *dirname = "roms";
        char *romfilenames[MAXROMS];

        /* Store current directory to return to later */
        if (getcwd(olddir, sizeof(olddir)) == NULL) {
                fatal("getcwd() failed: %s", strerror(errno));
        }

        /* Change into roms directory */
        append_filename(fn, rpcemu_get_datadir(), dirname, sizeof(fn));
        if (chdir(fn))
        {
                fatal("Cannot find roms directory '%s': %s", fn,
                      strerror(errno));
        }

        /* Scan directory for ROM files */
        finished=al_findfirst(wildcard,&ff,0xFFFF&~FA_DIREC);
        while (!finished && number_of_files < MAXROMS)
        {
                ext=get_extension(ff.name);
                /* Skip files with a .txt extension or starting with '.' */
                if (stricmp(ext,"txt") && ff.name[0] != '.')
                {
                        romfilenames[number_of_files] = strdup(ff.name);
                        if (romfilenames[number_of_files] == NULL) {
                                fatal("Out of memory in loadroms()");
                        }
                        number_of_files++;
                }
                finished = al_findnext(&ff);
        }
        al_findclose(&ff);

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

                f = fopen(romfilenames[c], "rb");
                if (f == NULL) {
                        fatal("Can't open ROM file '%s': %s", romfilenames[c],
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

        /* Return to initial directory */
        if (chdir(olddir)) {
                fatal("Cannot return to previous directory '%s': %s", olddir,
                      strerror(errno));
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

#ifdef _RPCEMU_BIG_ENDIAN /*Byte swap*/
#error "It's defined..."
//printf("Byte swapping...\n");
		for (c=0;c<0x800000;c+=4)
		{
                                uint32_t temp;
				temp=rom[c>>2];
				temp=((temp&0xFF000000)>>24)|((temp&0x00FF0000)>>8)|((temp&0x0000FF00)<<8)|((temp&0x000000FF)<<24);
//				temp=((temp>>24)&0xFF)|((temp>>8)&0xFF00)|((temp<<8)&0xFF0000)|((temp<<24)|0xFF000000);
				rom[c>>2]=temp;
		}
#endif
        /*Patch ROM for 8 meg vram!*/
        if (rom[0x14820>>2]==0xE3560001 && /*Check for ROS 4.02 startup*/
            rom[0x14824>>2]==0x33A02050 &&
            rom[0x14828>>2]==0x03A02004 &&
            rom[0x1482C>>2]==0x83A02008)
           rom[0x14824>>2]=0xE3A06008; /*MOV R6,#8 - 8 megs*/

//        initpodulerom();
}
