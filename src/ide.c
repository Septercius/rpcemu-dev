/*RPCemu v0.3 by Tom Walker
  IDE emulation*/
#include <stdio.h>
#include "rpc.h"

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
} ide;

int idereset=0;
unsigned short idebuffer[256];
unsigned char *idebufferb;
FILE *hdfile[2]={NULL,NULL};
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
}

void writeidew(uint16_t val)
{
//        rpclog("Write data %08X %04X\n",ide.pos,val);
        idebuffer[ide.pos>>1]=val;
        ide.pos+=2;
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
                return;
                case 0x1F7: /*Command register*/
                ide.command=val;
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
                        ide.atastat=0x80;
                        idecallback=200;
                        return;
                        case 0xEC: /*Identify device*/
                        ide.atastat=0x80;
                        idecallback=200;
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
//        rpclog("Read IDE %08X %08X\n",addr,PC-8);
        switch (addr)
        {
                case 0x1F0:
//                rpclog("Read data %08X\n",ide.pos);
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
                if (ide.pos>=512)
                {
                        ide.pos=0;
                        ide.atastat=0x40;
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
//        rpclog("Read data2 %08X %02X%02X\n",ide.pos,idebuffer[(ide.pos>>1)+1],idebuffer[(ide.pos>>1)]);
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
                case 0xE3:
                ide.atastat=0x41;
                ide.error=4;
                iomd.statb|=2;
                updateirqs();
                return;
                case 0xEC:
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
                idebufferb[49^1]='3';
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
        }
}
/*Read 1F1*/
/*Error &108A1 - parameters not recognised*/
