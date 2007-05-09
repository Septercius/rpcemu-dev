/*RPCemu v0.6 by Tom Walker
  IDE emulation*/
#include <stdio.h>
#include <stdint.h>
#include "rpcemu.h"
#include "vidc20.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"

int skip512[2];
int cdromenabled=0;
void atapicommand();
int timetolive;
int dumpedread=0;
struct
{
        unsigned char atastat;
        unsigned char error,status;
        int secount,sector,cylinder,head,drive,cylprecomp;
        unsigned char command;
        unsigned char fdisk;
        int pos;
        int packlen;
        int spt[2],hpc[2];
        int packetstatus;
} ide;

int idereset=0;
unsigned short idebuffer[2048];
unsigned char *idebufferb;
FILE *hdfile[2]={NULL,NULL};
FILE *cdrom=NULL;
void closeide0(void)
{
        fclose(hdfile[0]);
}

void closeide1(void)
{
        fclose(hdfile[1]);
}

void resetide(void)
{
        ide.atastat=0x40;
        idecallback=0;
        if (!hdfile[0])
        {
                hdfile[0]=fopen("hd4.hdf","rb+");
                if (!hdfile[0])
                {
                        hdfile[0]=fopen("hd4.hdf","wb");
                        putc(0,hdfile[0]);
                        fclose(hdfile[0]);
                        hdfile[0]=fopen("hd4.hdf","rb+");
                }
                atexit(closeide0);
        }
        idebufferb=(unsigned char *)idebuffer;
        fseek(hdfile[0],0xFC1,SEEK_SET);
        ide.spt[0]=getc(hdfile[0]);
        ide.hpc[0]=getc(hdfile[0]);
        skip512[0]=1;
//        rpclog("First check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
        if (!ide.spt[0] || !ide.hpc[0])
        {
                fseek(hdfile[0],0xDC1,SEEK_SET);
                ide.spt[0]=getc(hdfile[0]);
                ide.hpc[0]=getc(hdfile[0]);
//                rpclog("Second check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
                skip512[0]=0;
                if (!ide.spt[0] || !ide.hpc[0])
                {
                        ide.spt[0]=63;
                        ide.hpc[0]=16;
                        skip512[0]=1;
//        rpclog("Final check - spt %i hpc %i\n",ide.spt[0],ide.hpc[0]);
                }
        }
        if (!hdfile[1])
        {
                hdfile[1]=fopen("hd5.hdf","rb+");
                if (!hdfile[1])
                {
                        hdfile[1]=fopen("hd5.hdf","wb");
                        putc(0,hdfile[1]);
                        fclose(hdfile[1]);
                        hdfile[1]=fopen("hd5.hdf","rb+");
                }
                atexit(closeide1);
        }
        fseek(hdfile[1],0xFC1,SEEK_SET);
        ide.spt[1]=getc(hdfile[1]);
        ide.hpc[1]=getc(hdfile[1]);
        skip512[1]=1;
        if (!ide.spt[1] || !ide.hpc[1])
        {
                fseek(hdfile[1],0xDC1,SEEK_SET);
                ide.spt[1]=getc(hdfile[1]);
                ide.hpc[1]=getc(hdfile[1]);
                skip512[1]=0;
                if (!ide.spt[1] || !ide.hpc[1])
                {
                        ide.spt[1]=63;
                        ide.hpc[1]=16;
                        skip512[1]=1;
                }
        }
//        idebufferb=(unsigned char *)idebuffer;
//        ide.spt=63;
//        ide.hpc=16;
//        cdrom=fopen("d://au_cd8.iso","rb");
}

void writeidew(uint16_t val)
{
//        rpclog("Write data %08X %04X\n",ide.pos,val);
        idebuffer[ide.pos>>1]=val;
        ide.pos+=2;
        if (ide.command==0xA0 && ide.pos>=0xC)
        {
                ide.pos=0;
                ide.atastat=0x80;
                ide.packetstatus=1;
                idecallback=60;
        }
        if (ide.pos>=512)
        {
                ide.pos=0;
                ide.atastat=0x80;
                idecallback=1000;
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
                        ide.atastat=0x80;
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
                ide.drive=(val>>4)&1;
                        ide.pos=0;
                        ide.atastat=0x40;
                return;
                case 0x1F7: /*Command register*/
                ide.command=val;
//                rpclog("New IDE command - %02X %i %08X\n",ide.command,ide.drive,PC-8);
                ide.error=0;
                switch (val)
                {
                        case 0x10: /*Restore*/
                        case 0x70: /*Seek*/
                        ide.atastat=0x40;
                        idecallback=100;
                        return;
                        case 0x20: /*Read sector*/
/*                        if (ide.secount>1)
                        {
                                error("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                                exit(-1);
                        }*/
//                        rpclog("Read %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                        case 0x30: /*Write sector*/
/*                        if (ide.secount>1)
                        {
                                error("Write %i sectors to sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                                exit(-1);
                        }*/
//                        rpclog("Write %i sectors to sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat=0x08;
                        ide.pos=0;
                        return;
                        case 0x40: /*Read verify*/
//                        rpclog("Read verify %i sectors from sector %i cylinder %i head %i\n",ide.secount,ide.sector,ide.cylinder,ide.head);
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                        case 0x50:
//                        rpclog("Format track %i head %i\n",ide.cylinder,ide.head);
                        ide.atastat=0x08;
//                        idecallback=200;
                        ide.pos=0;
                        return;
                        case 0x91: /*Set parameters*/
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                        case 0xA1: /*Identify packet device*/
                        case 0xE3: /*Idle*/
  //                      case 0x08: /*???*/
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                        case 0xEC: /*Identify device*/
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                        case 0xA0: /*Packet (ATAPI command)*/
                        ide.packetstatus=0;
                        ide.atastat=0x80;
                        idecallback=30;
                        output=1;
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
                        ide.atastat=0x80;
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
//        rpclog("Read IDE %08X %08X %08X\n",addr,PC-8,armregs[9]);
        switch (addr)
        {
                case 0x1F0:
/*                if (ide.command==0xA1 && !ide.pos)
                {
                        output=1;
                        timetolive=20000;
                }*/
//                rpclog("Read data %08X ",ide.pos);
/*                if (!dumpedread)
                {
                        f=fopen("ram212.dmp","wb");
                        for (c=0x2127800;c<0x2127C00;c++)
                        {
                                putc(readmemb(c),f);
                        }
                        fclose(f);
                }*/
                temp=idebufferb[ide.pos++];
//                rpclog("%04X\n",temp);
                if (ide.pos>=512)
                {
                        ide.pos=0;
                        ide.atastat=0x40;
//                        rpclog("End of transfer\n");
                }
                return temp;
                case 0x1F1:
//                rpclog("Read IDEerror %02X\n",ide.atastat);
                return ide.error;
                case 0x1F2:
                return (uint8_t)ide.secount;
                case 0x1F3:
                return (uint8_t)ide.sector;
                case 0x1F4:
                return (uint8_t)(ide.cylinder&0xFF);
                case 0x1F5:
                return (uint8_t)(ide.cylinder>>8);
                case 0x1F6:
                return (uint8_t)(ide.head|(ide.drive<<4));
                case 0x1F7:
                iomd.statb&=~2;
                updateirqs();
//                rpclog("Read ATAstat %02X\n",ide.atastat);
                return ide.atastat;
                case 0x3F6:
//                rpclog("Read ATAstat %02X\n",ide.atastat);
                return ide.atastat;
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
        ide.pos+=2;
        if ((ide.pos>=512 && ide.command!=0xA0) || (ide.command==0xA0 && ide.pos>=ide.packlen))
        {
//                rpclog("Over! packlen %i %i\n",ide.packlen,ide.pos);
                ide.pos=0;
                ide.atastat=0x40;
                ide.packetstatus=0;
                if (ide.command==0x20)
                {
                        ide.secount--;
                        if (ide.secount)
                        {
                                ide.atastat=0x08;
                                ide.sector++;
                                if (ide.sector==(ide.spt[ide.drive]+1))
                                {
                                        ide.sector=1;
                                        ide.head++;
                                        if (ide.head==(ide.hpc[ide.drive]))
                                        {
                                                ide.head=0;
                                                ide.cylinder++;
                                        }
                                }
                                ide.atastat=0x80;
                                idecallback=200;
                        }
                }
        }
        return temp;
}

void callbackide(void)
{
        int c;
        int64_t addr;
        if (idereset)
        {
                ide.atastat=0x40;
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
                case 0x10: /*Restore*/
                case 0x70: /*Seek*/
//                rpclog("Restore callback\n");
                ide.atastat=0x40;
                iomd.statb|=2;
                updateirqs();
                return;
                case 0x20: /*Read sectors*/
//                rpclog("Read sector %i %i %i\n",ide.hpc[ide.drive],ide.spt[ide.drive],skip512[ide.drive]);
                readflash=1;
                addr=((((ide.cylinder*ide.hpc[ide.drive])+ide.head)*ide.spt[ide.drive])+(ide.sector-1)+skip512[ide.drive])*512;
//                rpclog("Read %i %i %i %08X\n",ide.cylinder,ide.head,ide.sector,addr);
                /*                if (ide.cylinder || ide.head)
                {
                        error("Read from other cylinder/head");
                        exit(-1);
                }*/
                fseeko64(hdfile[ide.drive],addr,SEEK_SET);
                fread(idebuffer,512,1,hdfile[ide.drive]);
                ide.pos=0;
                ide.atastat=0x08;
//                rpclog("Read sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                iomd.statb|=2;
                updateirqs();
                return;
                case 0x30: /*Write sector*/
                readflash=2;
                addr=((((ide.cylinder*ide.hpc[ide.drive])+ide.head)*ide.spt[ide.drive])+(ide.sector-1)+skip512[ide.drive])*512;
//                rpclog("Write sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                fseeko64(hdfile[ide.drive],addr,SEEK_SET);
                fwrite(idebuffer,512,1,hdfile[ide.drive]);
                iomd.statb|=2;
                updateirqs();
                ide.secount--;
                if (ide.secount)
                {
                        ide.atastat=0x08;
                        ide.pos=0;
                        ide.sector++;
                        if (ide.sector==(ide.spt[ide.drive]+1))
                        {
                                ide.sector=1;
                                ide.head++;
                                if (ide.head==(ide.hpc[ide.drive]))
                                {
                                        ide.head=0;
                                        ide.cylinder++;
                                }
                        }
                }
                else
                   ide.atastat=0x40;
                return;
                case 0x40: /*Read verify*/
                ide.pos=0;
                ide.atastat=0x40;
//                rpclog("Read verify callback %i %i %i offset %08X %i left\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount);
                iomd.statb|=2;
                updateirqs();
                return;
                case 0x50: /*Format track*/
                addr=((((ide.cylinder*ide.hpc[ide.drive])+ide.head)*ide.spt[ide.drive])+skip512[ide.drive])*512;
//                rpclog("Format cyl %i head %i offset %08X secount %I\n",ide.cylinder,ide.head,addr,ide.secount);
                fseeko64(hdfile[ide.drive],addr,SEEK_SET);
                memset(idebufferb,0,512);
                for (c=0;c<ide.secount;c++)
                {
                        fwrite(idebuffer,512,1,hdfile[ide.drive]);
                }
                ide.atastat=0x40;
                iomd.statb|=2;
                updateirqs();
                return;
                case 0x91: /*Set parameters*/
                ide.spt[ide.drive]=ide.secount;
                ide.hpc[ide.drive]=ide.head+1;
//                rpclog("%i sectors per track, %i heads per cylinder\n",ide.spt,ide.hpc);
                ide.atastat=0x40;
                iomd.statb|=2;
                updateirqs();
                return;
                case 0xA1:
                if (ide.drive)
                {
                        memset(idebuffer,0,512);
                        idebuffer[0]=0x8000|(5<<8)|0x80; /*ATAPI device, CD-ROM drive, removable media*/
                        for (addr=10;addr<20;addr++)
                            idebuffer[addr]=0x2020;
                        for (addr=23;addr<47;addr++)
                            idebuffer[addr]=0x2020;
                        idebufferb[46^1]='v'; /*Firmware version*/
                        idebufferb[47^1]='0';
                        idebufferb[48^1]='.';
                        idebufferb[49^1]='6';
                        idebufferb[54^1]='R'; /*Drive model*/
                        idebufferb[55^1]='P';
                        idebufferb[56^1]='C';
                        idebufferb[57^1]='e';
                        idebufferb[58^1]='m';
                        idebufferb[59^1]='u';
                        idebufferb[60^1]='C';
                        idebufferb[61^1]='D';
                        idebuffer[49]=0x200; /*LBA supported*/
                        ide.pos=0;
                        ide.error=0;
                        ide.atastat=0x08;
                        iomd.statb|=2;
                        updateirqs();
                        return;
                }
//                return;
                case 0xE3:
                ide.atastat=0x41;
                ide.error=4;
                iomd.statb|=2;
                updateirqs();
                return;
                case 0xEC:
                if (ide.drive && cdromenabled)
                {
                        ide.secount=1;
                        ide.sector=1;
                        ide.cylinder=0xEB14;
                        ide.drive=ide.head=0;
                        ide.atastat=0x41;
                        ide.error=4;
                        iomd.statb|=2;
                        updateirqs();
                        return;
                }
                memset(idebuffer,0,512);
                idebuffer[1]=101; /*Cylinders*/
                idebuffer[3]=16;  /*Heads*/
                idebuffer[6]=63;  /*Sectors*/
                for (addr=10;addr<20;addr++)
                    idebuffer[addr]=0x2020;
                for (addr=23;addr<47;addr++)
                    idebuffer[addr]=0x2020;
                idebufferb[46^1]='v'; /*Firmware version*/
                idebufferb[47^1]='0';
                idebufferb[48^1]='.';
                idebufferb[49^1]='6';
                idebufferb[54^1]='R'; /*Drive model*/
                idebufferb[55^1]='P';
                idebufferb[56^1]='C';
                idebufferb[57^1]='e';
                idebufferb[58^1]='m';
                idebufferb[59^1]='u';
                idebufferb[60^1]='H';
                idebufferb[61^1]='D';
                idebuffer[50]=0x4000; /*Capabilities*/
                ide.pos=0;
                ide.atastat=0x08;
//                rpclog("ID callback\n");
                iomd.statb|=2;
                updateirqs();
                return;
                case 0xA0: /*Packet*/
                if (!ide.packetstatus)
                {
                        ide.pos=0;
                        ide.error=(uint8_t)((ide.secount&0xF8)|1);
                        ide.atastat=8;
                        iomd.statb|=2;
                        updateirqs();
                        rpclog("Preparing to recieve packet max DRQ count %04X\n",ide.cylinder);
                }
                else if (ide.packetstatus==1)
                {
                        rpclog("packetstatus != 0!\n");
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X ",idebufferb[c]);
                        rpclog("\n");
                        ide.atastat=0x80;
                        
                        atapicommand();
//                        exit(-1);
                }
                else if (ide.packetstatus==2)
                {
                        rpclog("packetstatus==2\n");
                        ide.atastat=0x40;
                        iomd.statb|=2;
                        updateirqs();
                }
                else if (ide.packetstatus==3)
                {
                        ide.atastat=8;
                        rpclog("Recieve data packet!\n");
                        iomd.statb|=2;
                        updateirqs();
                        ide.packetstatus=4;
                }
                return;
        }
}

/*ATAPI CD-ROM emulation
  This is incomplete and hence disabled by default. In order to use it you have
  to set cdromenabled to 1, and open an ISO image to cdrom (see resetide()).
  Also the ISO length (blocks) is hardwired to the length of the AU_CD8 image.
  This is not usable in RISC OS at the moment due to the Mode Sense command not
  being implemented - the documentation I have is quite ambiguous. I'm not
  entirely sure what RISC OS wants from it.
  */
unsigned char atapibuffer[256];
int atapilen;

void atapicommand()
{
        int c;
        int len;
        int msf;
        int blocks=95932;
        rpclog("New ATAPI command %02X\n",idebufferb[0]);
                msf=idebufferb[1]&2;
        switch (idebufferb[0])
        {
                case 0: /*Test unit ready*/
                ide.packetstatus=2;
                idecallback=50;
                break;
                case 0x43: /*Read TOC*/
                len=4;
                if (idebufferb[6]>1 && idebufferb[6]!=0xAA)
                {
                        rpclog("Read bad track %02X in read TOC\n",idebufferb[6]);
                        rpclog("Packet data :\n");
                        for (c=0;c<12;c++)
                            rpclog("%02X ",idebufferb[c]);
                        rpclog("\n");
                        exit(-1);
                }

        if (idebufferb[6] <= 1) {
          idebufferb[len++] = 0; // Reserved
          idebufferb[len++] = 0x14; // ADR, control
          idebufferb[len++] = 1; // Track number
          idebufferb[len++] = 0; // Reserved

          // Start address
          if (msf) {
            idebufferb[len++] = 0; // reserved
            idebufferb[len++] = 0; // minute
            idebufferb[len++] = 2; // second
            idebufferb[len++] = 0; // frame
          } else {
            idebufferb[len++] = 0;
            idebufferb[len++] = 0;
            idebufferb[len++] = 0;
            idebufferb[len++] = 0; // logical sector 0
          }
        }

                idebufferb[2]=idebufferb[3]=1; /*First and last track numbers*/
        idebufferb[len++] = 0; // Reserved
        idebufferb[len++] = 0x16; // ADR, control
        idebufferb[len++] = 0xaa; // Track number
        idebufferb[len++] = 0; // Reserved

        if (msf) {
          idebufferb[len++] = 0; // reserved
          idebufferb[len++] = (uint8_t)(((blocks + 150) / 75) / 60); // minute
          idebufferb[len++] = (uint8_t)(((blocks + 150) / 75) % 60); // second
          idebufferb[len++] = (uint8_t)((blocks + 150) % 75); // frame;
        } else {
          idebufferb[len++] = (uint8_t)((blocks >> 24) & 0xff);
          idebufferb[len++] = (uint8_t)((blocks >> 16) & 0xff);
          idebufferb[len++] = (uint8_t)((blocks >> 8) & 0xff);
          idebufferb[len++] = (uint8_t)((blocks >> 0) & 0xff);
        }
        idebufferb[0] = (uint8_t)(((len-2) >> 8) & 0xff);
        idebufferb[1] = (uint8_t)((len-2) & 0xff);

        rpclog("ATAPI buffer len %i\n",len);
        for (c=0;c<len;c++) rpclog("%02X ",idebufferb[c]);
        rpclog("\n");
        ide.packetstatus=3;
        ide.cylinder=len;
        ide.secount=2;
//        ide.atastat=8;
        ide.pos=0;
                idecallback=60;
                ide.packlen=len;
        rpclog("Sending packet\n");
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
                printf("Read CD : start LBA %02X%02X%02X%02X Length %02X%02X%02X Flags %02X\n",idebufferb[2],idebufferb[3],idebufferb[4],idebufferb[5],idebufferb[6],idebufferb[7],idebufferb[8],idebufferb[9]);
                if (idebufferb[9]!=0x10)
                {
                        rpclog("Bad flags bits %02X\n",idebufferb[9]);
                        exit(-1);
                }
                if (idebufferb[6] || idebufferb[7] || (idebufferb[8]!=1))
                {
                        rpclog("More than 1 sector!\n");
                        exit(-1);
                }
                c=(idebufferb[2]<<24)|(idebufferb[3]<<16)|(idebufferb[4]<<8)|idebufferb[5];
                fseek(cdrom,c*2048,SEEK_SET);
                fread(idebufferb,2048,1,cdrom);
                ide.packetstatus=3;
                ide.cylinder=2048;
                ide.secount=2;
                ide.pos=0;
                idecallback=60;
                ide.packlen=2048;
                return;
                
                case 0x44: /*Read Header*/
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
                
                default:
                rpclog("Bad ATAPI command %02X\n",idebufferb[0]);
                rpclog("Packet data :\n");
                for (c=0;c<12;c++)
                    rpclog("%02X\n",idebufferb[c]);
                exit(-1);
        }
}


void filecoresectorop()
{
        int c,d;
        uint32_t temp[256];
        fseek(hdfile[(armregs[2]>>29)&1],((armregs[2]&0x1FFFFFFF)<<9)+512,SEEK_SET);
        for (c=armregs[4];c>0;c-=512)
        {
                fread(temp,512,1,hdfile[(armregs[2]>>29)&1]);
                for (d=0;d<((c>=512)?512:c);d+=4)
                    writememl(armregs[3]+d,temp[d>>2]);
//                temp=getc(hdfile[0]);
//                writememb(armregs[3],temp);
                armregs[3]+=d;
        }
//        armregs[3]+=armregs[4];
        armregs[2]+=armregs[4];
        armregs[4]=0;
}
