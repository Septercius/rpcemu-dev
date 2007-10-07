#include <stdio.h>
#include "rpcemu.h"
#include "ide.h"
#include "cdrom-iso.h"

ATAPI iso_atapi;

int iso_discchanged=0;
FILE *iso_file;
int iso_empty=0;
int iso_ready()
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

void iso_readsector(uint8_t *b, int sector)
{
        if (iso_empty) return;
        fseek(iso_file,sector*2048,SEEK_SET);
        fread(b,2048,1,iso_file);
}

int iso_readtoc(unsigned char *b, unsigned char starttrack, int msf)
{
        int len=4;
        int blocks;
        if (iso_empty) return 0;
        fseek(iso_file,-1,SEEK_END);
        blocks=ftell(iso_file)/2048;
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

uint8_t iso_getcurrentsubchannel(uint8_t *b, int msf)
{
        memset(b,0,2048);
        return 0;
}

void iso_playaudio(uint32_t pos, uint32_t len)
{
}

void iso_seek(uint32_t pos)
{
}

void iso_null()
{
}

int iso_open(char *fn)
{
        iso_file=fopen(fn,"rb");
        atapi=&iso_atapi;
        iso_discchanged=1;
        iso_empty=0;
        return 0;
}

void iso_close()
{
        if (iso_file) fclose(iso_file);
}

void iso_exit()
{
        if (iso_file) fclose(iso_file);
}

void iso_init()
{
        iso_empty=1;
        atapi=&iso_atapi;
}

ATAPI iso_atapi=
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
