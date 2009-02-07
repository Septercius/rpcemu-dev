/*RPCemu v0.6 by Tom Walker
  IDE emulation*/

void callbackide(void);

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include "rpcemu.h"
#include "vidc20.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"

ATAPI *atapi;

static void callreadcd();
static int skip512[4];
int cdromenabled=1;
static void atapicommand();
int timetolive;

static struct
{
        unsigned char atastat[4];
        unsigned char error;
        int secount,sector,cylinder,head,drive,cylprecomp;
        unsigned char command;
        unsigned char fdisk;
        int pos;
        int packlen;
        int spt[4],hpc[4];
        int packetstatus;
        int cdpos,cdlen;
        unsigned char asc;
        int discchanged;
        int board;
} ide;

int ideboard;
static int idereset;
static unsigned short idebuffer[65536];
static unsigned char *idebufferb;
static FILE *hdfile[4];

static inline void
ide_irq_raise(void)
{
	iomd.statb |= 2;
	updateirqs();
}

static inline void
ide_irq_lower(void)
{
	iomd.statb &= ~2;
	updateirqs();
}

/**
 * Copy a string into a buffer, padding with spaces, and placing characters as
 * if they were packed into 16-bit values, stored little-endian.
 *
 * @param str Destination buffer
 * @param src Source string
 * @param len Length of destination buffer to fill in. Strings shorter than
 *            this length will be padded with spaces.
 */
static void
ide_padstr(char *str, const char *src, int len)
{
	int i, v;

	for (i = 0; i < len; i++) {
		if (*src != '\0') {
			v = *src++;
		} else {
			v = ' ';
		}
		str[i ^ 1] = v;
	}
}

/**
 * Copy a string into a buffer, padding with spaces. Does not add string
 * terminator.
 *
 * @param buf      Destination buffer
 * @param buf_size Size of destination buffer to fill in. Strings shorter than
 *                 this length will be padded with spaces.
 * @param src      Source string
 */
static void
ide_padstr8(uint8_t *buf, int buf_size, const char *src)
{
	int i;

	for (i = 0; i < buf_size; i++) {
		if (*src != '\0') {
			buf[i] = *src++;
		} else {
			buf[i] = ' ';
		}
	}
}

/**
 * Fill in idebuffer with the output of the "IDENTIFY DEVICE" command
 */
static void
ide_identify(void)
{
	memset(idebuffer, 0, 512);

	//idebuffer[1] = 101; /* Cylinders */
	idebuffer[1] = 65535; /* Cylinders */
	idebuffer[3] = 16;  /* Heads */
	idebuffer[6] = 63;  /* Sectors */
	ide_padstr((char *) (idebuffer + 10), "", 20); /* Serial Number */
	ide_padstr((char *) (idebuffer + 23), "v1.0", 8); /* Firmware */
	ide_padstr((char *) (idebuffer + 27), "RPCEmuHD", 40); /* Model */
	idebuffer[50] = 0x4000; /* Capabilities */
}

/**
 * Fill in idebuffer with the output of the "IDENTIFY PACKET DEVICE" command
 */
static void
ide_atapi_identify(void)
{
	memset(idebuffer, 0, 512);

	idebuffer[0] = 0x8000 | (5<<8) | 0x80; /* ATAPI device, CD-ROM drive, removable media */
	ide_padstr((char *) (idebuffer + 10), "", 20); /* Serial Number */
	ide_padstr((char *) (idebuffer + 23), "v1.0", 8); /* Firmware */
	ide_padstr((char *) (idebuffer + 27), "RPCEmuCD", 40); /* Model */
	idebuffer[49] = 0x200; /* LBA supported */
}

static void loadhd(int d, const char *fn)
{
        if (!hdfile[d])
        {
                hdfile[d]=fopen(fn,"rb+");
                if (!hdfile[d])
                {
                        hdfile[d]=fopen64(fn,"rb+");
                        if (!hdfile[d])
                        {
                                hdfile[d]=fopen(fn,"wb");
                                if (!hdfile[d]) fatal("Cannot create file %s", fn);
                                putc(0,hdfile[d]);
                                fclose(hdfile[d]);
                                hdfile[d]=fopen64(fn,"rb+");
                                if (!hdfile[d]) fatal("Cannot open file %s", fn);
                        }
                }
        }
        fseek(hdfile[d],0xFC1,SEEK_SET);
        ide.spt[d]=getc(hdfile[d]);
        ide.hpc[d]=getc(hdfile[d]);
        skip512[d]=1;
//        rpclog("First check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
        if (!ide.spt[d] || !ide.hpc[d])
        {
                fseek(hdfile[d],0xDC1,SEEK_SET);
                ide.spt[d]=getc(hdfile[d]);
                ide.hpc[d]=getc(hdfile[d]);
//                rpclog("Second check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
                skip512[d]=0;
                if (!ide.spt[d] || !ide.hpc[d])
                {
                        ide.spt[d]=63;
                        ide.hpc[d]=16;
                        skip512[d]=1;
//        rpclog("Final check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
                }
        }
}

void resetide(void)
{
        int d;

        idebufferb=(unsigned char *)idebuffer;

        /* Close hard disk image files (if previously open) */
        for (d = 0; d < 4; d++) {
                if (hdfile[d]) {
                        fclose(hdfile[d]);
                        hdfile[d] = NULL;
                }
        }

        ide.atastat[0]=ide.atastat[2]=0x40;
        idecallback=0;
        loadhd(0,"hd4.hdf");
        if (cdromenabled)
        {
                /* Hard disk images for ICS IDE disabled
                loadhd(2,"hd5.hdf");
                loadhd(3,"hd6.hdf");
                */
        }
        else
        {
                loadhd(1,"hd5.hdf");
                /* Hard disk images for ICS IDE disabled
                loadhd(2,"hd6.hdf");
                loadhd(3,"hd7.hdf");
                */
        }
}

void writeidew(uint16_t val)
{
//        rpclog("Write data %08X %04X %i %07X %08X\n",ide.pos,val,ide.packetstatus,PC,armregs[5]);
#ifdef _RPCEMU_BIG_ENDIAN
		val=(val>>8)|(val<<8);
#endif
        idebuffer[ide.pos>>1]=val;
//        if (ide.command==0xA0) rpclog("Write packet %i %02X %02X %08X %08X\n",ide.pos,idebufferb[ide.pos],idebufferb[ide.pos+1],idebuffer,idebufferb);
        ide.pos+=2;
        if (ide.packetstatus==4)
        {
                if (ide.pos>=(ide.packlen+2))
                {
                        ide.packetstatus=5;
                        idecallback=6;
//                        rpclog("Packet over!\n");
                        if (!ide.board)
                        {
                                ide_irq_lower();
                        }
                }
                return;
        }
        else if (ide.packetstatus==5) return;
        else if (ide.command==0xA0 && ide.pos>=0xC)
        {
                ide.pos=0;
                ide.atastat[ide.board]=0x80;
                ide.packetstatus=1;
                idecallback=60;
//                rpclog("Packet now waiting!\n");
        }
        else if (ide.pos>=512)
        {
                ide.pos=0;
                ide.atastat[ide.board]=0x80;
                idecallback=0;
                callbackide();
        }
}

void writeide(uint16_t addr, uint8_t val)
{
//        int c;
//        rpclog("Write IDE %08X %02X %08X %08X\n",addr,val,PC-8,armregs[12]);
        switch (addr)
        {
                case 0x1F0:
                idebufferb[ide.pos++]=val;
                if (ide.pos>=512)
                {
                        ide.pos=0;
                        ide.atastat[ide.board]=0x80;
                        idecallback=1000;
                }
                return;
                case 0x1F1:
                ide.cylprecomp=val;
                return;
                case 0x1F2:
                ide.secount=val;
                return;
                case 0x1F3:
                ide.sector=val;
                return;
                case 0x1F4:
                ide.cylinder=(ide.cylinder&0xFF00)|val;
                return;
                case 0x1F5:
                ide.cylinder=(ide.cylinder&0xFF)|(val<<8);
                return;
                case 0x1F6:
                ide.head=val&0xF;
                if (((val>>4)&1)!=ide.drive)
                {
                        idecallback=0;
                        ide.atastat[ide.board]=0x40;
                        ide.error=0;
                        ide.secount=1;
                        ide.sector=1;
                        ide.head=0;
                        ide.cylinder=0;
                        ide.cylprecomp=0;
                        idereset=0;
                        ide.command=0;
                        ide.packetstatus=0;
                        ide.packlen=0;
                        ide.cdlen=ide.cdpos=ide.pos=0;
                        memset(idebuffer,0,512);
                        if (!ide.board)
                        {
                                ide_irq_lower();
                        }
                }
                ide.drive=(val>>4)&1;
                        ide.pos=0;
                        ide.atastat[ide.board]=0x40;
                return;
                case 0x1F7: /*Command register*/
                ide.command=val;
                ide.board=ideboard;
//                rpclog("New IDE command - %02X %i %i %08X\n",ide.command,ide.drive,ide.board,PC-8);
                ide.error=0;
                switch (val)
                {
                        case 0x08: /*Device reset*/
                        ide.atastat[ide.board]=0x40;
                        idecallback=100;
                        return;
                        case 0x10: /*Restore*/
                        case 0x70: /*Seek*/
                        ide.atastat[ide.board]=0x40;
                        idecallback=100;
                        return;
                        case 0x20: /*Read sector*/
/*                        if (ide.secount>1)
                        {
                                error("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                                exit(-1);
                        }*/
//                        rpclog("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat[ide.board]=0x80;
                        idecallback=200;
                        return;
                        case 0x30: /*Write sector*/
/*                        if (ide.secount>1)
                        {
                                error("Write %i sectors to sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                                exit(-1);
                        }*/
//                        rpclog("Write %i sectors to sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat[ide.board]=0x08;
                        ide.pos=0;
                        return;
                        case 0x40: /*Read verify*/
//                        rpclog("Read verify %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat[ide.board]=0x80;
                        idecallback=200;
                        return;
                        case 0x50:
//                        rpclog("Format track %i head %i\n",ide.cylinder,ide.head);
                        ide.atastat[ide.board]=0x08;
//                        idecallback=200;
                        ide.pos=0;
                        return;
                        case 0x91: /*Set parameters*/
                        ide.atastat[ide.board]=0x80;
                        idecallback=200;
                        return;
                        case 0xA1: /*Identify packet device*/
                        case 0xE3: /*Idle*/
  //                      case 0x08: /*???*/
                        ide.atastat[ide.board]=0x80;
                        idecallback=200;
                        return;
                        case 0xEC: /*Identify device*/
                        ide.atastat[ide.board]=0x80;
                        idecallback=200;
                        return;
                        case 0xA0: /*Packet (ATAPI command)*/
                        ide.packetstatus=0;
                        ide.atastat[ide.board]=0x80;
                        idecallback=30;
                        ide.pos=0;
//                        output=1;
                        return;
                }
                error("Bad IDE command %02X\n",val);
                dumpregs();
                exit(-1);
                return;
                case 0x3F6:
                if ((ide.fdisk&4) && !(val&4))
                {
                        idecallback=500;
                        idereset=1;
                        ide.atastat[ide.board]=0x80;
//                        rpclog("IDE Reset\n");
                }
                ide.fdisk=val;
                return;
        }
        error("Bad IDE write %04X %02X\n",addr,val);
        dumpregs();
        exit(-1);
}

uint8_t readide(uint16_t addr)
{
        uint8_t temp;
//        FILE *f;
//        int c;
//        if (output==1) rpclog("Read IDE %08X %08X %08X\n",addr,PC-8,armregs[9]);
        switch (addr)
        {
                case 0x1F0:
/*                if (ide.command==0xA1 && !ide.pos)
                {
                        output=1;
                        timetolive=20000;
                }*/
//                rpclog("Read data %08X ",ide.pos);
                temp=idebufferb[ide.pos++];
//                rpclog("%04X\n",temp);
                if (ide.pos>=512)
                {
                        ide.pos=0;
                        ide.atastat[ide.board]=0x40;
//                        rpclog("End of transfer\n");
                }
                return temp;
                case 0x1F1:
//                rpclog("Read IDEerror %02X %02X\n",ide.atastat,ide.error);
                return ide.error;
                case 0x1F2:
                return (uint8_t)ide.secount;
                case 0x1F3:
                return (uint8_t)ide.sector;
                case 0x1F4:
//                        rpclog("Read cylinder low %02X\n",ide.cylinder&0xFF);
                return (uint8_t)(ide.cylinder&0xFF);
                case 0x1F5:
//                        rpclog("Read cylinder high %02X\n",ide.cylinder>>8);
                return (uint8_t)(ide.cylinder>>8);
                case 0x1F6:
                return (uint8_t)(ide.head|(ide.drive<<4));
                case 0x1F7:
                if (!ide.board)
                {
                        ide_irq_lower();
                }
//                rpclog("Read ATAstat %02X\n",ide.atastat);
                return ide.atastat[ide.board];
                case 0x3F6:
//                rpclog("Read ATAstat %02X\n",ide.atastat);
                return ide.atastat[ide.board];
        }
        error("Bad IDE read %04X\n",addr);
        dumpregs();
        exit(-1);
}

uint16_t readidew(void)
{
        uint16_t temp;
//        if (ide.command==0xA0) rpclog("Read data2 %08X %04X %07X\n",ide.pos,idebuffer[(ide.pos>>1)],PC);
//        if (output) rpclog("Read data2 %08X %02X%02X %07X\n",ide.pos,idebuffer[(ide.pos>>1)+1],idebuffer[(ide.pos>>1)],PC);
        temp=idebuffer[ide.pos>>1];
	#ifdef _RPCEMU_BIG_ENDIAN
		temp=(temp>>8)|(temp<<8);
	#endif
        ide.pos+=2;
        if ((ide.pos>=512 && ide.command!=0xA0) || (ide.command==0xA0 && ide.pos>=ide.packlen))
        {
//                rpclog("Over! packlen %i %i\n",ide.packlen,ide.pos);
                ide.pos=0;
                if (ide.command==0xA0 && ide.packetstatus==6)
                {
                        callreadcd();
                }
                else
                {
                        ide.atastat[ide.board]=0x40;
                        ide.packetstatus=0;
                        if (ide.command==0x20)
                        {
                                ide.secount--;
                                if (ide.secount)
                                {
                                        ide.atastat[ide.board]=0x08;
                                        ide.sector++;
                                        if (ide.sector==(ide.spt[ide.drive|ide.board]+1))
                                        {
                                                ide.sector=1;
                                                ide.head++;
                                                if (ide.head==(ide.hpc[ide.drive|ide.board]))
                                                {
                                                        ide.head=0;
                                                        ide.cylinder++;
                                                }
                                        }
                                        ide.atastat[ide.board]=0x80;
                                        idecallback=0;
                                        callbackide();
                                }
                        }
                }
        }
        return temp;
}

void callbackide(void)
{
        off64_t addr;
        int c;
        if (idereset)
        {
                ide.atastat[ide.board]=0x40;
                ide.error=0;
                ide.secount=1;
                ide.sector=1;
                ide.head=0;
                ide.cylinder=0;
                idereset=0;
//                rpclog("Reset callback\n");
                return;
        }
        switch (ide.command)
        {
                case 0x08: /*Device reset*/
                ide.atastat[ide.board]=0x40;
                ide.error=1; /*Device passed*/
                if ((ide.drive|ide.board)!=1 || !cdromenabled)
                {
                        ide.secount=ide.sector=1;
                        ide.cylinder=0;
                }
                else
                {
                        ide.secount=ide.sector=1;
                        ide.cylinder=0xEB14;
                }
                if (!ide.board)
                {
                        ide_irq_raise();
                }
                return;
                case 0x10: /*Restore*/
                case 0x70: /*Seek*/
//                rpclog("Restore callback\n");
                ide.atastat[ide.board]=0x40;
                if (!ide.board)
                {
                        ide_irq_raise();
                }
                return;
                case 0x20: /*Read sectors*/
//                rpclog("Read sector %i %i %i\n",ide.hpc[ide.drive],ide.spt[ide.drive],skip512[ide.drive]);
                addr = (((((off64_t) ide.cylinder * ide.hpc[ide.drive|ide.board]) +  ide.head) * ide.spt[ide.drive|ide.board]) + (ide.sector - 1) + skip512[ide.drive|ide.board]) * 512;
//                rpclog("Read %i %i %i %08X\n",ide.cylinder,ide.head,ide.sector,addr);
                /*                if (ide.cylinder || ide.head)
                {
                        error("Read from other cylinder/head");
                        exit(-1);
                }*/
                fseeko64(hdfile[ide.drive|ide.board],addr,SEEK_SET);
                fread(idebuffer,512,1,hdfile[ide.drive|ide.board]);
                ide.pos=0;
                ide.atastat[ide.board]=0x08;
//                rpclog("Read sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                if (!ide.board)
                {
                        ide_irq_raise();
                }
                return;
                case 0x30: /*Write sector*/
                addr = (((((off64_t) ide.cylinder * ide.hpc[ide.drive|ide.board]) +  ide.head) * ide.spt[ide.drive|ide.board]) + (ide.sector - 1) + skip512[ide.drive|ide.board]) * 512;
//                rpclog("Write sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                fseeko64(hdfile[ide.drive|ide.board],addr,SEEK_SET);
                fwrite(idebuffer,512,1,hdfile[ide.drive|ide.board]);
                if (!ide.board)
                {
                        ide_irq_raise();
                }
                ide.secount--;
                if (ide.secount)
                {
                        ide.atastat[ide.board]=0x08;
                        ide.pos=0;
                        ide.sector++;
                        if (ide.sector==(ide.spt[ide.drive|ide.board]+1))
                        {
                                ide.sector=1;
                                ide.head++;
                                if (ide.head==(ide.hpc[ide.drive|ide.board]))
                                {
                                        ide.head=0;
                                        ide.cylinder++;
                                }
                        }
                }
                else
                   ide.atastat[ide.board]=0x40;
                return;
                case 0x40: /*Read verify*/
                ide.pos=0;
                ide.atastat[ide.board]=0x40;
//                rpclog("Read verify callback %i %i %i offset %08X %i left\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount);
                if (!ide.board)
                {
                        ide_irq_raise();
                }
                return;
                case 0x50: /*Format track*/
                addr = (((((off64_t) ide.cylinder * ide.hpc[ide.drive|ide.board]) +  ide.head) * ide.spt[ide.drive|ide.board]) + (ide.sector - 1) + skip512[ide.drive|ide.board]) * 512;
//                rpclog("Format cyl %i head %i offset %08X %08X %08X secount %i\n",ide.cylinder,ide.head,addr,addr>>32,addr,ide.secount);
                fseeko64(hdfile[ide.drive|ide.board],addr,SEEK_SET);
                memset(idebufferb,0,512);
                for (c=0;c<ide.secount;c++)
                {
                        fwrite(idebuffer,512,1,hdfile[ide.drive|ide.board]);
                }
                ide.atastat[ide.board]=0x40;
                if (!ide.board)
                {
                        ide_irq_raise();
                }
                return;
                case 0x91: /*Set parameters*/
                ide.spt[ide.drive|ide.board]=ide.secount;
                ide.hpc[ide.drive|ide.board]=ide.head+1;
                ide.atastat[ide.board]=0x40;
//                rpclog("%i sectors per track, %i heads per cylinder\n",ide.spt,ide.hpc);
                if (!ide.board)
                {
                        ide_irq_raise();
                }
                return;
                case 0xA1:
                if (ide.drive && !ide.board && cdromenabled)
                {
                        ide_atapi_identify();
                        ide.pos=0;
                        ide.error=0;
                        ide.atastat[ide.board]=0x08;
                        ide_irq_raise();
                        return;
                }
//                return;
                case 0xE3:
                ide.atastat[ide.board]=0x41;
                ide.error=4;
                if (!ide.board)
                {
                        ide_irq_raise();
                }
                return;
                case 0xEC:
                if (ide.drive && cdromenabled && !ide.board)
                {
                        ide.secount=1;
                        ide.sector=1;
                        ide.cylinder=0xEB14;
                        ide.drive=ide.head=0;
                        ide.atastat[ide.board]=0x41;
                        ide.error=4;
                        ide_irq_raise();
                        return;
                }
                ide_identify();
                ide.pos=0;
                ide.atastat[ide.board]=0x08;
//                rpclog("ID callback\n");
                if (!ide.board)
                {
                        ide_irq_raise();
                }
                return;
                case 0xA0: /*Packet*/
//                rpclog("Packet callback! %i\n",ide.packetstatus);
                if (!ide.packetstatus)
                {
                        ide.pos=0;
                        ide.error=(uint8_t)((ide.secount&0xF8)|1);
                        ide.atastat[ide.board]=8;
                        if (!ide.board)
                        {
                                ide_irq_raise();
                        }
//                        rpclog("Preparing to recieve packet max DRQ count %04X\n",ide.cylinder);
                }
                else if (ide.packetstatus==1)
                {
/*                        rpclog("packetstatus != 0!\n");
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X ",idebufferb[c]);
                        rpclog("\n");*/
                        ide.atastat[ide.board]=0x80;
                        
                        atapicommand();
//                        exit(-1);
                }
                else if (ide.packetstatus==2)
                {
//                        rpclog("packetstatus==2\n");
                        ide.atastat[ide.board]=0x40;
                        if (!ide.board)
                        {
                                ide_irq_raise();
                        }
//                        if (output)
//                        {
//                                output=2;
//                                timetolive=10000;
//                        }
                }
                else if (ide.packetstatus==3)
                {
                        ide.atastat[ide.board]=8;
//                        rpclog("Recieve data packet!\n");
                        if (!ide.board)
                        {
                                ide_irq_raise();
                        }
                        ide.packetstatus=0xFF;
                }
                else if (ide.packetstatus==4)
                {
                        ide.atastat[ide.board]=8;
//                        rpclog("Send data packet!\n");
                        if (!ide.board)
                        {
                                ide_irq_raise();
                        }
//                        ide.packetstatus=5;
                        ide.pos=2;
                }
                else if (ide.packetstatus==5)
                {
//                        rpclog("Packetstatus 5 !\n");
                        atapicommand();
                }
                else if (ide.packetstatus==6) /*READ CD callback*/
                {
                        ide.atastat[ide.board]=8;
//                        rpclog("Recieve data packet 6!\n");
                        if (!ide.board)
                        {
                                ide_irq_raise();
                        }
//                        ide.packetstatus=0xFF;
                }
                else if (ide.packetstatus==0x80) /*Error callback*/
                {
                        ide.atastat[ide.board]=0x41;
                        if (!ide.board)
                        {
                                ide_irq_raise();
                        }
                }
                return;
        }
}

/*ATAPI CD-ROM emulation
  This mostly seems to work. It is implemented only on Windows at the moment as
  I haven't had time to implement any interfaces for it in the generic gui.
  It mostly depends on driver files - cdrom-iso.c for ISO image files (in theory
  on any platform) and cdrom-ioctl.c for Win32 IOCTL access. There's an ATAPI
  interface defined in ide.h.
  There are a couple of bugs in the CD audio handling.
  */

static void atapi_notready()
{
        /*Medium not present is 02/3A/--*/
        /*cylprecomp is error number*/
        /*SENSE/ASC/ASCQ*/
        ide.atastat[0]=0x41;    /*CHECK CONDITION*/
        ide.error=(2 <<4)|4; /*Unit attention*/
        if (ide.discchanged) ide.error|=8;
        ide.discchanged=0;
        ide.asc=0x3A;
        ide.packetstatus=0x80;
        idecallback=50;
}

void atapi_discchanged()
{
        ide.discchanged=1;
}
/*Tell RISC OS that we have a 4x CD-ROM drive (600kb/sec data, 706kb/sec raw).
  Not that it means anything*/
static int cdromspeed = 706;
static void atapicommand()
{
        int c;
        int len;
        int msf;
        int pos=0;
//        rpclog("New ATAPI command %02X\n",idebufferb[0]);
                msf=idebufferb[1]&2;
        switch (idebufferb[0])
        {
                case 0: /*Test unit ready*/
                if (!atapi->ready()) { atapi_notready(); return; }
//                if (atapi->ready())
//                {
                        ide.packetstatus=2;
                        idecallback=50;
//                }
//                else
//                {
//                        rpclog("Medium not present!\n");
//                }
                break;
                case 0x03: /*Read sense - used by ROS 4+*/
                /*Will return 18 bytes of 0*/
                memset(idebufferb,0,512);
                ide.packetstatus=3;
                ide.cylinder=18;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=18;
                break;

                case 0xBB: /*Set CD speed*/
                ide.packetstatus=2;
                idecallback=50;
//                output=1;
                break;
                case 0x43: /*Read TOC*/
                if (!atapi->ready()) { atapi_notready(); return; }
                if (idebufferb[9]>>7)
                {
                        rpclog("Bad read TOC format\n");
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X ",idebufferb[c]);
                        rpclog("\n");
                        exit(-1);
                }
                len=atapi->readtoc(idebufferb,idebufferb[6],msf);
  /*      rpclog("ATAPI buffer len %i\n",len);
        for (c=0;c<len;c++) rpclog("%02X ",idebufferb[c]);
        rpclog("\n");*/
        ide.packetstatus=3;
        ide.cylinder=len;
        ide.secount=2;
//        ide.atastat=8;
        ide.pos=0;
                idecallback=60;
                ide.packlen=len;
//        rpclog("Sending packet\n");
        return;
        
                switch (idebufferb[6])
                {
                        case 0xAA: /*Start address of lead-out*/
                        break;
                }
                rpclog("Read bad track %02X in read TOC\n",idebufferb[6]);
                rpclog("Packet data :\n");
                for (c=0;c<12;c++)
                    rpclog("%02X\n",idebufferb[c]);
                exit(-1);
                
                case 0xBE: /*Read CD*/
                if (!atapi->ready()) { atapi_notready(); return; }
//                rpclog("Read CD : start LBA %02X%02X%02X%02X Length %02X%02X%02X Flags %02X\n",idebufferb[2],idebufferb[3],idebufferb[4],idebufferb[5],idebufferb[6],idebufferb[7],idebufferb[8],idebufferb[9]);
                if (idebufferb[9]!=0x10)
                {
                        rpclog("Bad flags bits %02X\n",idebufferb[9]);
                        exit(-1);
                }
/*                if (idebufferb[6] || idebufferb[7] || (idebufferb[8]!=1))
                {
                        rpclog("More than 1 sector!\n");
                        exit(-1);
                }*/
                ide.cdlen=(idebufferb[6]<<16)|(idebufferb[7]<<8)|idebufferb[8];
                ide.cdpos=(idebufferb[2]<<24)|(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
//                rpclog("Read at %08X %08X\n",ide.cdpos,ide.cdpos*2048);
                atapi->readsector(idebufferb,ide.cdpos);

                ide.cdpos++;
                ide.cdlen--;
                if (ide.cdlen>=0) ide.packetstatus=6;
                else              ide.packetstatus=3;
                ide.cylinder=2048;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=2048;
                return;
                
                case 0x44: /*Read Header*/
                if (!atapi->ready()) { atapi_notready(); return; }
                if (msf)
                {
                        rpclog("Read Header MSF!\n");
                        exit(-1);
                }
                for (c=0;c<4;c++) idebufferb[c+4]=idebufferb[c+2];
                idebufferb[0]=1; /*2048 bytes user data*/
                idebufferb[1]=idebufferb[2]=idebufferb[3]=0;
                
                ide.packetstatus=3;
                ide.cylinder=8;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=8;
                return;
                
                case 0x5A: /*Mode Sense*/
                if (!atapi->ready()) { atapi_notready(); return; }
                if (idebufferb[2]!=0x3F)
                {
                        rpclog("Bad mode sense - not 3F\n");
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X\n",idebufferb[c]);
                        exit(-1);
                }
                len=(idebufferb[8]|(idebufferb[7]<<8));
//                rpclog("Mode sense! %i\n",len);
                for (c=0;c<len;c++) idebufferb[c]=0;
                /*Set mode parameter header - bytes 0 & 1 are data length (filled out later),
                  byte 2 is media type*/
                idebufferb[2]=1; /*120mm data CD-ROM*/
                pos=8;
                /*&01 - Read error recovery*/
                idebufferb[pos++]=0x01; /*Page code*/
                idebufferb[pos++]=6; /*Page length*/
                idebufferb[pos++]=0; /*Error recovery parameters*/
                idebufferb[pos++]=3; /*Read retry count*/
                idebufferb[pos++]=0; /*Reserved*/
                idebufferb[pos++]=0; /*Reserved*/
                idebufferb[pos++]=0; /*Reserved*/
                idebufferb[pos++]=0; /*Reserved*/
                /*&0D - CD-ROM Parameters*/
                idebufferb[pos++]=0x0D; /*Page code*/
                idebufferb[pos++]=6; /*Page length*/
                idebufferb[pos++]=0; /*Reserved*/
                idebufferb[pos++]=1; /*Inactivity time multiplier *NEEDED BY RISCOS* value is a guess*/
                idebufferb[pos++]=0; idebufferb[pos++]=60; /*MSF settings*/
                idebufferb[pos++]=0; idebufferb[pos++]=75; /*MSF settings*/
                /*&2A - CD-ROM capabilities and mechanical status*/
                idebufferb[pos++]=0x2A; /*Page code*/
                idebufferb[pos++]=0x12; /*Page length*/
                idebufferb[pos++]=0; idebufferb[pos++]=0; /*CD-R methods*/
                idebufferb[pos++]=1; /*Supports audio play, not multisession*/
                idebufferb[pos++]=0; /*Some other stuff not supported*/
                idebufferb[pos++]=0; /*Some other stuff not supported (lock state + eject)*/
                idebufferb[pos++]=0; /*Some other stuff not supported*/
                idebufferb[pos++]=cdromspeed>>8; idebufferb[pos++]=cdromspeed&0xFF; /*Maximum speed - 706kpbs (4x)*/
                idebufferb[pos++]=0; idebufferb[pos++]=2; /*Number of audio levels - on and off only*/
                idebufferb[pos++]=0; idebufferb[pos++]=0; /*Buffer size - none*/
                idebufferb[pos++]=706>>8; idebufferb[pos++]=706&0xFF; /*Current speed - 706kpbs (4x)*/
                idebufferb[pos++]=0; /*Reserved*/
                idebufferb[pos++]=0; /*Drive digital format*/
                idebufferb[pos++]=0; /*Reserved*/
                idebufferb[pos++]=0; /*Reserved*/
                len=pos;
                idebufferb[0]=len>>8;
                idebufferb[1]=len&255;
/*        rpclog("ATAPI buffer len %i\n",len);
        for (c=0;c<len;c++) rpclog("%02X ",idebufferb[c]);
        rpclog("\n");*/
        ide.packetstatus=3;
        ide.cylinder=len;
        ide.secount=2;
//        ide.atastat=8;
        ide.pos=0;
                idecallback=60;
                ide.packlen=len;
//        rpclog("Sending packet\n");
        return;

                case 0x55: /*Mode select*/
                if (!atapi->ready()) { atapi_notready(); return; }
                if (ide.packetstatus==5)
                {
                        ide.atastat[0]=0;
//                        rpclog("Recieve data packet!\n");
                        ide_irq_raise();
                        ide.packetstatus=0xFF;
                        ide.pos=0;
                }
                else
                {
                        len=(idebufferb[7]<<8)|idebufferb[8];
                        ide.packetstatus=4;
                        ide.cylinder=len;
                        ide.secount=2;
                        ide.pos=0;
                        idecallback=6;
                        ide.packlen=len;
/*                        rpclog("Waiting for ARM to send packet %i\n",len);
                rpclog("Packet data :\n");
                for (c=0;c<12;c++)
                    rpclog("%02X ",idebufferb[c]);
                    rpclog("\n");*/
//                    output=1;
//                        output=2;
//                        timetolive=200000;
                }
                return;

                case 0xA5: /*Play audio (12)*/
                if (!atapi->ready()) { atapi_notready(); return; }
                /*This is apparently deprecated in the ATAPI spec, and apparently
                  has been since 1995 (!). Hence I'm having to guess most of it*/
                pos=(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
                len=(idebufferb[7]<<16)|(idebufferb[8]<<8)|idebufferb[9];
                atapi->playaudio(pos,len);
                ide.packetstatus=2;
                idecallback=50;
                break;

                case 0x42: /*Read subchannel*/
                if (!atapi->ready()) { atapi_notready(); return; }
                if (idebufferb[3]!=1)
                {
                        rpclog("Bad read subchannel!\n");
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X\n",idebufferb[c]);
                        dumpregs();
                        exit(-1);
                }
                pos=0;
                idebufferb[pos++]=0;
                idebufferb[pos++]=0; /*Audio status*/
                idebufferb[pos++]=0; idebufferb[pos++]=0; /*Subchannel length*/
                idebufferb[pos++]=1; /*Format code*/
                idebufferb[1]=atapi->getcurrentsubchannel(&idebufferb[5],msf);
                len=11+5;
                ide.packetstatus=3;
                ide.cylinder=len;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=len;
                break;

                case 0x1B: /*Start/stop unit*/
                if (idebufferb[4]!=2 && idebufferb[4]!=3 && idebufferb[4])
                {
                        rpclog("Bad start/stop unit command\n");
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X\n",idebufferb[c]);
                        exit(-1);
                }
                if (!idebufferb[4])        atapi->stop();
                else if (idebufferb[4]==2) atapi->eject();
                else                       atapi->load();
                ide.packetstatus=2;
                idecallback=50;
                break;
                
                case 0x12: /*Inquiry*/
                idebufferb[0] = 5; /*CD-ROM*/
                idebufferb[1] = 0;
                idebufferb[2] = 0;
                idebufferb[3] = 0;
                idebufferb[4] = 31;
                idebufferb[5] = 0;
                idebufferb[6] = 0;
                idebufferb[7] = 0;
                ide_padstr8(idebufferb + 8, 8, "RPCEmu"); /* Vendor */
                ide_padstr8(idebufferb + 16, 16, "RPCEmuCD"); /* Product */
                ide_padstr8(idebufferb + 32, 4, "v1.0"); /* Revision */

                len=36;
                ide.packetstatus=3;
                ide.cylinder=len;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=len;
                break;
                
                case 0x4B: /*Pause/resume unit*/
                if (!atapi->ready()) { atapi_notready(); return; }
                if (idebufferb[8]&1) atapi->resume();
                else                 atapi->pause();
                ide.packetstatus=2;
                idecallback=50;
                break;

                case 0x2B: /*Seek*/
                if (!atapi->ready()) { atapi_notready(); return; }
                pos=(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
                atapi->seek(pos);
                ide.packetstatus=2;
                idecallback=50;
                break;

                case 0xAD: /*???*/
                default:
                ide.atastat[0]=0x41;    /*CHECK CONDITION*/
                ide.error=(5 <<4)|4;    /*Illegal command*/
                if (ide.discchanged) ide.error|=8;
                ide.discchanged=0;
                ide.asc=0x20;
                ide.packetstatus=0x80;
                idecallback=50;
                break;
                
/*                default:
                rpclog("Bad ATAPI command %02X\n",idebufferb[0]);
                rpclog("Packet data :\n");
                for (c=0;c<12;c++)
                    rpclog("%02X\n",idebufferb[c]);
                exit(-1);*/
        }
}

static void callreadcd()
{
        ide_irq_lower();
        if (ide.cdlen<=0)
        {
                ide.packetstatus=2;
                idecallback=20;
                return;
        }
//        rpclog("Continue readcd! %i blocks left\n",ide.cdlen);
        ide.atastat[0]=0x80;
        
                atapi->readsector(idebufferb,ide.cdpos);

                ide.cdpos++;
                ide.cdlen--;
                ide.packetstatus=6;
                ide.cylinder=2048;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=2048;
        output=1;
}
