#include <stdio.h>
#include <linux/cdrom.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "rpcemu.h"
#include "ide.h"

ATAPI ioctl_atapi;

int ioctl_discchanged=0;
int ioctl_empty=0;
int ioctl_ready()
{
	int cdrom=open("/dev/cdrom",O_RDONLY|O_NONBLOCK);
        if (cdrom<=0) return 0;
	return 1;
}

void ioctl_readsector(uint8_t *b, int sector)
{
	int cdrom=open("/dev/cdrom",O_RDONLY|O_NONBLOCK);
        if (cdrom<=0) return;
        lseek(cdrom,sector*2048,SEEK_SET);
        read(cdrom,b,2048);
	close(cdrom);
}

/*I'm not sure how to achieve this properly, so the TOC is faked*/
int ioctl_readtoc(unsigned char *b, unsigned char starttrack, int msf)
{
        int len=4;
        int blocks;
	int cdrom=open("/dev/cdrom",O_RDONLY|O_NONBLOCK);
        if (cdrom<=0) return 0;
	close(cdrom);
        blocks=(600*1024*1024)/2048;
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

uint8_t ioctl_getcurrentsubchannel(uint8_t *b, int msf)
{
        memset(b,0,2048);
        return 0;
}

void ioctl_playaudio(uint32_t pos, uint32_t len)
{
}

void ioctl_seek(uint32_t pos)
{
}

void ioctl_null()
{
}

int ioctl_open()
{
        atapi=&ioctl_atapi;
        ioctl_discchanged=1;
        ioctl_empty=0;
        return 0;
}

void ioctl_close()
{
}

void ioctl_exit()
{
}

void ioctl_init()
{
        ioctl_empty=1;
        atapi=&ioctl_atapi;
}

ATAPI ioctl_atapi=
{
        ioctl_ready,
        ioctl_readtoc,
        ioctl_getcurrentsubchannel,
        ioctl_readsector,
        ioctl_playaudio,
        ioctl_seek,
        ioctl_null,ioctl_null,ioctl_null,ioctl_null,ioctl_null,
        ioctl_exit
};
