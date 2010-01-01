#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include "rpcemu.h"
#include "ide.h"
#include "cdrom-iso.h"

static ATAPI iso_atapi;

static int iso_discchanged = 0;
static FILE *iso_file;
static int iso_empty = 0;

static int iso_ready(void)
{
        if (iso_empty) return 0;
        if (iso_discchanged)
        {
                iso_discchanged=0;
                atapi_discchanged();
                return 0;
        }
        return 1;
}

static void iso_readsector(uint8_t *b, int sector)
{
        if (iso_empty) return;
        fseeko64(iso_file, (off64_t) sector * 2048, SEEK_SET);
        fread(b,2048,1,iso_file);
}

static int iso_readtoc(unsigned char *b, unsigned char starttrack, int msf)
{
        int len=4;
        int blocks;
        if (iso_empty) return 0;
        fseeko64(iso_file, 0, SEEK_END);
        blocks = (int) (ftello64(iso_file) / 2048);
        if (starttrack <= 1) {
          b[len++] = 0; // Reserved
          b[len++] = 0x14; // ADR, control
          b[len++] = 1; // Track number
          b[len++] = 0; // Reserved

          // Start address
          if (msf) {
            b[len++] = 0; // reserved
            b[len++] = 0; // minute
            b[len++] = 2; // second
            b[len++] = 0; // frame
          } else {
            b[len++] = 0;
            b[len++] = 0;
            b[len++] = 0;
            b[len++] = 0; // logical sector 0
          }
        }

        b[2]=b[3]=1; /*First and last track numbers*/
        b[len++] = 0; // Reserved
        b[len++] = 0x16; // ADR, control
        b[len++] = 0xaa; // Track number
        b[len++] = 0; // Reserved

        if (msf) {
          b[len++] = 0; // reserved
          b[len++] = (uint8_t)(((blocks + 150) / 75) / 60); // minute
          b[len++] = (uint8_t)(((blocks + 150) / 75) % 60); // second
          b[len++] = (uint8_t)((blocks + 150) % 75); // frame;
        } else {
          b[len++] = (uint8_t)((blocks >> 24) & 0xff);
          b[len++] = (uint8_t)((blocks >> 16) & 0xff);
          b[len++] = (uint8_t)((blocks >> 8) & 0xff);
          b[len++] = (uint8_t)((blocks >> 0) & 0xff);
        }
        b[0] = (uint8_t)(((len-4) >> 8) & 0xff);
        b[1] = (uint8_t)((len-4) & 0xff);
        return len;
}

static uint8_t iso_getcurrentsubchannel(uint8_t *b, int msf)
{
        memset(b,0,2048);
        return 0;
}

static void iso_playaudio(uint32_t pos, uint32_t len)
{
}

static void iso_seek(uint32_t pos)
{
}

static void iso_null(void)
{
}

int
iso_open(const char *fn)
{
	atapi = &iso_atapi;

	iso_file = fopen64(fn, "rb");
	if (iso_file != NULL) {
		/* Successfully opened ISO file */
		iso_empty = 0;
	} else {
		/* Failed to open ISO file - behave as if drive empty */
		iso_empty = 1;
	}
	iso_discchanged = 1;
	return 0;
}

void iso_close(void)
{
        if (iso_file) fclose(iso_file);
}

static void iso_exit(void)
{
        if (iso_file) fclose(iso_file);
}

void iso_init(void)
{
        iso_empty=1;
        atapi=&iso_atapi;
}

static ATAPI iso_atapi=
{
        iso_ready,
        iso_readtoc,
        iso_getcurrentsubchannel,
        iso_readsector,
        iso_playaudio,
        iso_seek,
        iso_null,iso_null,iso_null,iso_null,iso_null,
        iso_exit
};
