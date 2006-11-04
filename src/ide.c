/*RPCemu v0.5 by Tom Walker
  IDE emulation*/
#include <stdio.h>
#include "rpcemu.h"

int cdromenabled=0;
void atapicommand();
int timetolive;
int readflash;
int dumpedread=0;
struct
{
        unsigned char atastat;
        unsigned char error,status;
        int secount,sector,cylinder,head,drive,cylprecomp;
        unsigned char command;
        unsigned char fdisk;
        int pos;
        int spt,hpc;
        int packetstatus;
} ide;

int idereset=0;
unsigned short idebuffer[256];
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
        idebufferb=(unsigned char *)idebuffer;
        ide.spt=63;
        ide.hpc=16;
//        cdrom=fopen("","rb");
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
//        if (output) rpclog("Write IDE %08X %02X %08X %08X\n",addr,val,PC-8,armregs[12]);
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
//        if (output) rpclog("Read IDE %08X %08X %08X\n",addr,PC-8,armregs[9]);
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
                return ide.secount;
                case 0x1F3:
                return ide.sector;
                case 0x1F4:
                return ide.cylinder&0xFF;
                case 0x1F5:
                return ide.cylinder>>8;
                case 0x1F6:
                return ide.head|(ide.drive<<4);
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
//        if (output) rpclog("Read data2 %08X %02X%02X %07X\n",ide.pos,idebuffer[(ide.pos>>1)+1],idebuffer[(ide.pos>>1)],PC);
        temp=idebuffer[ide.pos>>1];
        ide.pos+=2;
        if (ide.pos>=512)
        {
                ide.pos=0;
                ide.atastat=0x40;
                if (ide.command==0x20)
                {
                        ide.secount--;
                        if (ide.secount)
                        {
                                ide.atastat=0x08;
                                ide.sector++;
                                if (ide.sector==(ide.spt+1))
                                {
                                        ide.sector=1;
                                        ide.head++;
                                        if (ide.head==(ide.hpc))
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
        int addr,c;
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
                readflash=1;
                addr=((((ide.cylinder*ide.hpc)+ide.head)*ide.spt)+(ide.sector))*512;
                /*                if (ide.cylinder || ide.head)
                {
                        error("Read from other cylinder/head");
                        exit(-1);
                }*/
                fseek(hdfile[ide.drive],addr,SEEK_SET);
                fread(idebuffer,512,1,hdfile[ide.drive]);
                ide.pos=0;
                ide.atastat=0x08;
//                rpclog("Read sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                iomd.statb|=2;
                updateirqs();
                return;
                case 0x30: /*Write sector*/
                readflash=2;
                addr=((((ide.cylinder*ide.hpc)+ide.head)*ide.spt)+(ide.sector))*512;
//                rpclog("Write sector callback %i %i %i offset %08X %i left %i\n",ide.sector,ide.cylinder,ide.head,addr,ide.secount,ide.spt);
                fseek(hdfile[ide.drive],addr,SEEK_SET);
                fwrite(idebuffer,512,1,hdfile[ide.drive]);
                iomd.statb|=2;
                updateirqs();
                ide.secount--;
                if (ide.secount)
                {
                        ide.atastat=0x08;
                        ide.pos=0;
                        ide.sector++;
                        if (ide.sector==(ide.spt+1))
                        {
                                ide.sector=1;
                                ide.head++;
                                if (ide.head==(ide.hpc))
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
                addr=(((ide.cylinder*ide.hpc)+ide.head)*ide.spt)*512;
//                rpclog("Format cyl %i head %i offset %08X secount %I\n",ide.cylinder,ide.head,addr,ide.secount);
                fseek(hdfile[ide.drive],addr,SEEK_SET);
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
                ide.spt=ide.secount;
                ide.hpc=ide.head+1;
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
                        ide.error=(ide.secount&0xF8)|1;
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
                            rpclog("%02X\n",idebufferb[c]);
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
                return;
        }
}
/*Read 1F1*/
/*Error &108A1 - parameters not recognised*/

void atapicommand()
{
        int c;
        switch (idebufferb[0])
        {
                case 0: /*Test unit ready*/
                ide.packetstatus=2;
                idecallback=50;
                break;
                case 0x43: /*Read TOC*/
                switch (idebuffer[6])
                {
                        case 0xAA: /*Start address of lead-out*/
                        break;
                }
                rpclog("Read bad track %02X in read TOC\n",idebuffer[6]);
                rpclog("Packet data :\n");
                for (c=0;c<12;c++)
                    rpclog("%02X\n",idebufferb[c]);
                exit(-1);
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
        unsigned long temp[256];
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
