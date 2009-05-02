/* NEC 765/Intel 82077 Floppy drive emulation, on RPC/A7000 a part of the
   SMC 37C665GT PC style Super IO chip */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "rpcemu.h"
#include "fdc.h"
#include "vidc20.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"

static void fdcsend(uint8_t val);

static struct
{
        uint8_t dor;
        int reset;
        uint8_t status;
        int incommand;
        uint8_t command;
        uint8_t st0,st1,st2,st3;
        int commandpos;
        int track,sector,side;
        uint8_t data;
        int params,curparam;
        uint8_t parameters[10];
        uint8_t dmadat;
        int rate;
        int oldpos;
} fdc;

static uint8_t disc[2][2][80][10][1024];
static int discdensity[2];
static int discsectors[2];
static int discchanged[2];

void fdc_reset(void)
{
        fdccallback=0;
}

void loadadf(const char *fn, int drive)
{
        FILE *f=fopen(fn,"rb");
        int h,t,s,b;
        if (!f) return;
        fseek(f,-1,SEEK_END);
        if (ftell(f)>1000000)
        {
                discdensity[drive]=0;
                discsectors[drive]=10;
        }
        else
        {
                discdensity[drive]=2;
                discsectors[drive]=5;
        }
        discchanged[drive]=0;
        fseek(f,0,SEEK_SET);
//        printf("Disc density %i sectors %i\n",discdensity[drive],discsectors[drive]);
        for (t=0;t<80;t++)
        {
                for (h=0;h<2;h++)
                {
                        for (s=0;s<discsectors[drive];s++)
                        {
//                                printf("Read drive %i track %i head %i sector %i\n",drive,t,h,s);
                                for (b=0;b<1024;b++)
                                {
                                        disc[drive][h][t][s][b]=getc(f);
                                }
                        }
                }
        }
        fclose(f);
}

void saveadf(const char *fn, int drive)
{
        FILE *f;
        int h,t,s,b;
        if (!discchanged[drive]) return;
        f=fopen(fn,"wb");
        if (!f) return;
        discchanged[drive]=0;
//        printf("Disc density %i sectors %i\n",discdensity[drive],discsectors[drive]);
        for (t=0;t<80;t++)
        {
                for (h=0;h<2;h++)
                {
                        for (s=0;s<discsectors[drive];s++)
                        {
//                                printf("Write track %i head %i sector %i\n",t,h,s);
                                for (b=0;b<1024;b++)
                                {
                                        putc(disc[drive][h][t][s][b],f);
                                }
                        }
                }
        }
        fclose(f);
}


void writefdc(uint32_t addr, uint32_t val)
{
        //printf("FDC write %03X %08X %08X\n", addr, val, PC);
        switch (addr)
        {
                case 0x3F2: /*DOR*/
                if ((val&4) && !(fdc.dor&4)) /*Reset*/
                {
                        fdc.reset=1;
                        fdccallback=500;
                }
                if (!(val&4)) fdc.status=0x80;
                if (val&0x10) motoron=1;
                break;
                case 0x3F4:
//                printf("3F4 write %02X %07X\n",val,PC);
                break;
                case 0x3F5: /*Command*/
                //output=0;
//                printf("Command write %02X %i : rate %i\n",val,ins,fdc.rate);
                timetolive=50;
                if (fdc.params)
                {
                        fdc.parameters[fdc.curparam++]=val;
//                        printf("Param : %i %i %02X\n",fdc.curparam,fdc.params,val);
                        if (fdc.curparam==fdc.params)
                        {
                                fdc.status&=~0x80;
                                switch (fdc.command)
                                {
                                        case 3: /*Specify*/
                                        fdccallback=100;
                                        break;
                                        case 4: /*Sense drive status*/
                                        fdccallback=100;
                                        break;
                                        case 7: /*Recalibrate*/
//                                        printf("Recalibrate starting\n");
                                        case 0x0F: /*Seek*/
                                        fdccallback=500;
                                        fdc.status|=1;
                                        break;
                                        case 0x13: /*Configure*/
                                        fdccallback=100;
                                        break;
                                        case 0x45: /*Write data - MFM*/
                                        fdc.commandpos=0;
                                        fdccallback=1000;
                                        fdc.st0=fdc.parameters[0]&7;
                                        fdc.st1=fdc.st2=0;
                                        fdc.track=fdc.parameters[1];
                                        fdc.side=fdc.parameters[2];
                                        fdc.sector=fdc.parameters[3];
//                                        rpclog("Write data %i %i %i\n",fdc.side,fdc.track,fdc.sector);
                                        discchanged[fdc.st0&1]=1;
                                        break;
                                        case 0x46: /*Read data - MFM*/
                                        fdc.commandpos=0;
                                        fdccallback=1000;
                                        fdc.st0=fdc.parameters[0]&7;
                                        fdc.st1=fdc.st2=0;
                                        fdc.track=fdc.parameters[1];
                                        fdc.side=fdc.parameters[2];
                                        fdc.sector=fdc.parameters[3];
//                                        printf("Read data %i %i %i\n",fdc.side,fdc.track,fdc.sector);
                                        break;
                                        case 0x4A: /*Read ID - MFM*/
                                        fdc.commandpos=0;
                                        fdccallback=4000;
                                        fdc.st0=fdc.parameters[0]&7;
                                        fdc.st1=fdc.st2=0;
                                        if (fdc.rate!=discdensity[fdc.st0&1]) fdc.command=0xA;
//                                        printf("Density : %i %i\n",fdc.rate,discdensity[0]);
                                        break;
                                        case 0x0A: /*Read ID - FM*/
                                        fdc.commandpos=0;
                                        fdccallback=4000;
                                        fdc.st0=fdc.parameters[0]&7;
                                        fdc.st1=fdc.st2=0;
                                        break;

                                        default:
                                        UNIMPLEMENTED("FDC command",
                                                      "Unknown command 0x%02x",
                                                      fdc.command);
                                }
                        }
                        return;
                }
                if (fdc.incommand)
                {
                        error("FDC already in command\n");
                        dumpregs();
                        exit(-1);
                }
                fdc.incommand=1;
                fdc.commandpos=0;
                fdc.command=val;
//                printf("Rate %i %i\n",discdensity[0],fdc.rate);
                switch (fdc.command)
                {
                        case 3: /*Specify*/
                        fdc.params=2;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 4: /*Sense drive status*/
                        fdc.params=1;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 7: /*Recalibrate*/
                        fdc.params=1;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 8: /*Sense interrupt status*/
                        fdccallback=100;
                        fdc.status=0x10;
                        break;

                        case 0xE: /*Dump registers - not supported by 82c711
                                    Used by Linux to identify FDC type.*/
                fdc.st0=0x80;
                fdcsend(fdc.st0);
                fdc.incommand=0;
                fdccallback=0;
                fdc.status=0x80;
                        break;
//                        fdccallback=50;
//                        fdc.status=0x10;
//                        break;

                        case 0x0F: /*Seek*/
                        fdc.params=2;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 0x13: /*Configure*/
                        fdc.params=3;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 0x45: /*Write data - MFM*/
                        case 0x46: /*Read data - MFM*/
                        fdc.params=8;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        case 0x0A: /*Read ID - FM*/
                        case 0x4A: /*Read ID - MFM*/
                        fdc.params=1;
                        fdc.curparam=0;
                        fdc.status=0x90;
                        break;

                        default:
                        UNIMPLEMENTED("FDC command 2",
                                      "Unknown command 0x%02x", fdc.command);
                        error("Bad FDC command %02X\n",val);
                        dumpregs();
                        exit(-1);
                }
                break;
                case 0x3F7:
                //output=0;
//                printf("3F7 write %02X %07X\n",val,PC);
                fdc.rate=val&3;
                break;

                default:
                UNIMPLEMENTED("FDC write",
                              "Unknown register 0x%03x", addr);
        }
}


uint8_t readfdc(uint32_t addr)
{
        //printf("FDC read %03X %08X\n", addr, PC);
        switch (addr)
        {
                case 0x3F4:
                iomd.irqb.status &= ~0x10;
                updateirqs();
//                printf("Status : %02X %07X\n",fdc.status,PC);
                return fdc.status;
                case 0x3F5: /*Data*/
/*                if (fdc.command==4)
                {
                        timetolive=400;
                }*/
                fdc.status&=0x7F;
                if (!fdc.incommand) fdc.status=0x80;
                else                fdccallback=100;
//                printf("Read FDC data %02X\n",fdc.data);
                return fdc.data;
//                case 0x3F7: return 0x80;

                default:
                UNIMPLEMENTED("FDC read",
                              "Unknown register 0x%03x", addr);
        }
        return 0;
}

static void fdcsend(uint8_t val)
{
//        printf("New FDC data %02X %02X %i %i\n",val,fdc.command,fdc.incommand,fdc.commandpos);
        fdc.data=val;
        fdc.status=0xD0;
        iomd.irqb.status |= 0x10;
        updateirqs();
}

static void fdcsend2(uint8_t val)
{
//        printf("NO INT - New FDC data %02X %02X %i %i\n",val,fdc.command,fdc.incommand,fdc.commandpos);
        fdc.data=val;
        fdc.status=0xD0;
}

static void fdcsenddata(uint8_t val)
{
//        printf("New FDC DMA data %02X %02X %i %i  %i %i %i\n",val,fdc.command,fdc.incommand,fdc.commandpos,fdc.side,fdc.track,fdc.sector);
        fdc.dmadat=val;
        iomd.fiq.status |= 1;
        updateirqs();
//        timetolive=50;
}


void callbackfdc(void)
{
  //        int maxsector=5;
        if (fdc.reset)
        {
                iomd.irqb.status |= 0x10;
                updateirqs();
                fdc.reset=0;
                fdc.status=0x80;
                fdc.incommand=0;
                fdc.st0=0xC0;
                fdc.track=0;
                fdc.curparam=fdc.params=0;
                fdc.rate=2;
                return;
        }
        switch (fdc.command)
        {
                case 3:
//                printf("Specify : %02X %02X\n",fdc.parameters[0],fdc.parameters[1]);
                fdc.incommand=0;
                fdc.status=0x80;
                fdc.params=fdc.curparam=0;
                break;
                case 4: /*Sense drive status*/
                fdc.st3=(fdc.parameters[0]&7)|0x28;
                if (!fdc.track) fdc.st3|=0x10;
                fdc.incommand=0;
//                printf("Send ST3\n");
//                timetolive=150;
                fdcsend(fdc.st3);
                fdc.params=fdc.curparam=0;
                break;
                case 7: /*Recalibrate*/
                fdc.track=0;
                fdc.incommand=0;
                fdc.status=0x80;
                fdc.params=fdc.curparam=0;
                iomd.irqb.status |= 0x10;
                updateirqs();
                fdc.st0=0x20;
//                printf("Recalibrate complete\n");
                break;
                case 8:
                fdc.commandpos++;
                if (fdc.commandpos==1)
                {
//                        printf("Send ST0\n");
                        fdcsend(fdc.st0);
                        fdccallback=100;
                }
                else
                {
//                        printf("Send track\n");
                        fdc.incommand=0;
                        fdcsend(fdc.track);
                }
                break;
//                case 0x0E: /*Dump registers - act as invalid command*/
//                fdc.st0=0x80;
//                fdcsend(fdc.st0);
//                fdc.incommand=0;
//                break;
                case 0x0F: /*Seek*/
//                printf("Seek to %i\n",fdc.parameters[1]);
                fdc.track=fdc.parameters[1];
                fdc.incommand=0;
                fdc.status=0x80;
                fdc.params=fdc.curparam=0;
                iomd.irqb.status |= 0x10;
                updateirqs();
                fdc.st0=0x20;
                break;
                case 0x13:
//                printf("Configure : %02X %02X %02X\n",fdc.parameters[0],fdc.parameters[1],fdc.parameters[2]);
                fdc.incommand=0;
                fdc.status=0x80;
                fdc.params=fdc.curparam=0;
                break;
                case 0x45: /*Write data - MFM*/
                if (fdc.commandpos==2048)
                {
                        disc[fdc.st0&1][fdc.side][fdc.track][fdc.sector][fdc.oldpos-1]=fdc.dmadat;
//                        rpclog("Write %i %02i %i %03X %02X\n",fdc.side,fdc.track,fdc.sector,fdc.oldpos-1,fdc.dmadat);
//                        rpclog("Operation terminated\n");
                        fdc.commandpos=1025;
                        fdccallback=500;
                        fdc.sector++;
                }
                else if (fdc.commandpos>=1025)
                {
//                        printf("Sending result\n");
//                        fdccallback=50;
                        switch (fdc.commandpos-1025)
                        {
                                case 0: fdcsend(fdc.st0); break;
                                case 1: fdcsend2(fdc.st1); break;
                                case 2: fdcsend2(fdc.st2); break;
                                case 3: fdcsend2(fdc.track); break;
                                case 4: fdcsend2((fdc.parameters[0]&4)?1:0); break;
                                case 5: fdcsend2(fdc.sector); break;
                                case 6:
                                fdcsend2(3);
                                fdc.incommand=0;
                                fdc.params=fdc.curparam=0;
                                fdccallback=0;
//                                printf("Ins %i\n",ins);
                                break;
                        }
                        fdc.commandpos++;
                }
                else
                {
                        if (fdc.commandpos)
                        {
                                disc[fdc.st0&1][fdc.side][fdc.track][fdc.sector][fdc.commandpos-1]=fdc.dmadat;
//                                rpclog("Write %i %02i %i %03X %02X\n",fdc.side,fdc.track,fdc.sector,fdc.commandpos-1,fdc.dmadat);
                        }
                        fdc.commandpos++;
                        if (fdc.commandpos==1025)
                        {
//                                rpclog("End of sector\n");
                                fdc.sector++;
                                if (fdc.sector<=fdc.parameters[5])
                                {
//                                        printf("Continuing to next sector\n");
                                        fdc.commandpos=0;
                                        fdccallback=100;
                                        return;
                                }
                                else
                                {
                                        fdccallback=100;
                                        return;
                                }
                        }
                        else
                        {
//                                printf("FIQ\n");
                                iomd.fiq.status |= 1;
                                updateirqs();
                        }
                        fdccallback=0;
                }
                break;
                case 0x46: /*Read data - MFM*/
//                printf("Read data callback %i\n",fdc.commandpos);
                if (fdc.commandpos>=1024)
                {
//                        printf("sending result %i\n",fdc.commandpos-1024);
//                        timetolive=500;
//                        fdccallback=20;
                        switch (fdc.commandpos-1024)
                        {
                                case 0: fdcsend(fdc.st0); break;
                                case 1: fdcsend2(fdc.st1); break;
                                case 2: fdcsend2(fdc.st2); break;
                                case 3: fdcsend2(fdc.track); break;
                                case 4: fdcsend2((fdc.parameters[0]&4)?1:0); break;
                                case 5: fdcsend2(fdc.sector); break;
                                case 6:
                                fdcsend2(3);
                                fdc.incommand=0;
                                fdc.params=fdc.curparam=0;
                                fdccallback=0;
//                                printf("Ins %i\n",ins);
                                break;
                        }
                        fdc.commandpos++;
                }
                else
                {
//                        printf("sending data\n");
                        fdcsenddata(disc[fdc.st0&1][fdc.side][fdc.track][fdc.sector][fdc.commandpos]);
                        fdc.commandpos++;
                        if (fdc.commandpos==1024)
                        {
//                                printf("Finished sector %i - target %i\n",fdc.sector,fdc.parameters[5]);
                                fdc.sector++;
                                if (fdc.sector<=fdc.parameters[5])
                                   fdc.commandpos=0;
/*                                else
                                {
//                                        printf("End of read op\n");
                                        fdc.sector=1;
                                }*/
                        }
                        fdccallback=0;
                }
                break;
                case 0x4A: /*Read ID - MFM*/
                if (fdc.sector>=discsectors[fdc.st0&1]) fdc.sector=0;
                switch (fdc.commandpos)
                {
                        case 0: fdcsend(fdc.st0); break;
                        case 1: fdcsend2(fdc.st1); break;
                        case 2: fdcsend2(fdc.st2); break;
                        case 3: fdcsend2(fdc.track); break;
                        case 4: fdcsend2((fdc.parameters[0]&4)?1:0); break;
                        case 5: fdcsend2(fdc.sector); break;
                        case 6: fdcsend2(3); break;
                        default:
                        printf("Bad ReadID command pos %i\n",fdc.commandpos);
                        exit(-1);
                }
                fdc.commandpos++;
                if (fdc.commandpos==7)
                {
//                        printf("Sector %i : maxsector %i density %i\n",fdc.sector,discsectors[0],discdensity[0]);
                        fdc.incommand=0;
//                        printf("Read ID for track %i sector %i\n",fdc.track,fdc.sector);
                        fdc.sector++;
                        if (fdc.sector>=discsectors[fdc.st0&1]) fdc.sector=0;
                        fdc.params=fdc.curparam=0;
                }
//                else
//                   fdccallback=50;
                break;
                case 0x0A:
                iomd.irqb.status |= 0x10;
                updateirqs();
                fdc.st0=0x40|(fdc.parameters[0]&7);
                fdc.st1=1;
                fdc.st2=1;
                fdc.incommand=0;
                fdc.params=fdc.curparam=0;
                break;
        }
}

uint8_t readfdcdma(uint32_t addr)
{
//        printf("Read FDC DMA %08X\n",addr);
        iomd.fiq.status &= ~1;
        updateirqs();
        fdccallback=100;
        if (!fdc.commandpos) fdccallback=2000;
        if (addr==0x302A000)
        {
//                if (!fdc.commandpos)
//                   fdc.sector--;
                fdc.commandpos=1024;
//                printf("End of DMA\n");
                fdccallback=2000;
                fdc.st0=0;
        }
        return fdc.dmadat;
}

void writefdcdma(uint32_t addr, uint8_t val)
{
        iomd.fiq.status &= ~1;
        updateirqs();
        fdccallback=200;
        if (!fdc.commandpos) fdccallback=2000;
        if (addr==0x302A000)
        {
//                printf("DMA terminated\n");
                fdc.oldpos=fdc.commandpos;
                fdc.commandpos=2048;
                fdccallback=2000;
                fdc.st0=0;
        }
        fdc.dmadat=val;
//        printf("Write DMA dat %02X %08X\n",val,addr);
}
