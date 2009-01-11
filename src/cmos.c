/*RPCemu v0.6 by Tom Walker
  I2C + CMOS RAM emulation*/
#include <stdint.h>
#include <allegro.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "rpcemu.h"
#include "cmos.h"

uint32_t output;
static int cmosstate = 0;
static int i2cstate = 0;
static int lastdata;
static unsigned char i2cbyte;
int i2cclock=1,i2cdata=1;
static int i2cpos;
static int i2ctransmit = -1;

#define CMOS 1
#define ARM -1

#define I2C_IDLE             0
#define I2C_RECIEVE          1
#define I2C_TRANSMIT         2
#define I2C_ACKNOWLEDGE      3
#define I2C_TRANSACKNOWLEDGE 4

#define CMOS_IDLE            0
#define CMOS_RECIEVEADDR     1
#define CMOS_RECIEVEDATA     2
#define CMOS_SENDDATA        3

#define BIN2BCD(val)	((((val) / 10) << 4) | ((val) % 10))

static unsigned char cmosaddr;
static unsigned char cmosram[256];
static int cmosrw;

static void cmosgettime(void);

void loadcmos()
{
        char fn[512];
        FILE *cmosf;

        append_filename(fn,exname,"cmos.ram",511);
        cmosf=fopen(fn,"rb");

	if (!cmosf) {
          fprintf(stderr, "Could not open CMOS file '%s': %s\n", fn, strerror(errno));
	  exit(EXIT_FAILURE);
        }

        fread(cmosram,256,1,cmosf);
        fclose(cmosf);

        /* Clear the bytes that correspond to registers (i.e. not NVRAM) */
        memset(cmosram, 0, 16);
}

void savecmos()
{
        char fn[512];
        FILE *cmosf;

        append_filename(fn,exname,"cmos.ram",511);
        cmosf=fopen("cmos.ram","wb");
//        for (c=0;c<256;c++)
//            putc(cmosram[(c-0x40)&0xFF],cmosf);
        fwrite(cmosram,256,1,cmosf);
        fclose(cmosf);
}

static void cmosstop(void)
{
        cmosstate=CMOS_IDLE;
        i2ctransmit=ARM;
}

static void cmosnextbyte(void)
{
        i2cbyte=cmosram[((cmosaddr++))&0xFF];
}

unsigned char cmosgetbyte()
{
        return cmosram[cmosaddr++];
}

static void cmosgettime(void)
{
	time_t now = time(NULL);
	const struct tm *t = gmtime(&now);

	cmosram[1] = 0;
	cmosram[2] = BIN2BCD(t->tm_sec);
	cmosram[3] = BIN2BCD(t->tm_min);
	cmosram[4] = BIN2BCD(t->tm_hour);
	cmosram[5] = (((t->tm_year + 1900) & 3) << 6) | BIN2BCD(t->tm_mday);
	cmosram[6] = (t->tm_wday << 5) | BIN2BCD(t->tm_mon + 1);
}

static void cmoswrite(unsigned char byte)
{
  //        char s[80];
        switch (cmosstate)
        {
                case CMOS_IDLE:
                cmosrw=byte&1;
                if (cmosrw)
                {
                        cmosstate=CMOS_SENDDATA;
                        i2ctransmit=CMOS;
						
                        /* If reading the clock, resync the 
                           byte variables with the current time */
                        if (cmosaddr < 0x10)
                                cmosgettime();

/*                        if (!olog) olog=fopen("olog.txt","wt");*/
//                        rpclog("%02X",cmosram[cmosaddr]);
//                        timetolive=5000;
/*                        fputs(s,olog);*/
                        i2cbyte=cmosram[((cmosaddr++))&0xFF];
//printf("CMOS - %02X from %02X\n",i2cbyte,cmosaddr-1);
//                        log("Transmitter now CMOS\n");
                }
                else
                {
                        cmosstate=CMOS_RECIEVEADDR;
                        i2ctransmit=ARM;
                }
//                log("CMOS R/W=%i\n",cmosrw);
                return;

                case CMOS_RECIEVEADDR:
//                printf("CMOS addr=%02X\n",byte);
//                log("CMOS addr=%02X\n",byte);
                cmosaddr=byte;
                if (cmosrw)
                   cmosstate=CMOS_SENDDATA;
                else
                   cmosstate=CMOS_RECIEVEDATA;
                break;

                case CMOS_RECIEVEDATA:
//                printf("CMOS write %02X %02X\n",cmosaddr,byte);
//                log("%02X now %02X\n",cmosaddr,byte);
//                        sprintf(s,"Write CMOS %02X - %02X\n",cmosaddr,byte);
//                        fputs(s,olog);
                cmosram[((cmosaddr++))&0xFF]=byte;
                break;

                case CMOS_SENDDATA:
//                closevideo();
                printf("Send data %02X\n",cmosaddr);
                exit(-1);
        }
}

void cmosi2cchange(int nuclock, int nudata)
{
//        if (i2cclock==nuclock && lastdata==nudata) return;
//        printf("I2C %i %i %i %i  %i\n",i2cclock,nuclock,i2cdata,nudata,i2cstate);
//        rpclog("I2C update clock %i %i data %i %i state %i\n",i2cclock,nuclock,i2cdata,nudata,i2cstate);
        switch (i2cstate)
        {
                case I2C_IDLE:
                if (i2cclock && nuclock)
                {
                        if (lastdata && !nudata) /*Start bit*/
                        {
//                                printf("Start bit\n");
//                                log("Start bit recieved\n");
                                i2cstate=I2C_RECIEVE;
                                i2cpos=0;
                        }
                }
                break;

                case I2C_RECIEVE:
                if (!i2cclock && nuclock)
                {
//                        printf("Reciving\n");
                        i2cbyte<<=1;
                        if (nudata)
                           i2cbyte|=1;
                        else
                           i2cbyte&=0xFE;
                        i2cpos++;
                        if (i2cpos==8)
                        {

//                                rpclog("Complete - byte %02X\n",i2cbyte);
                                cmoswrite(i2cbyte);
                                i2cstate=I2C_ACKNOWLEDGE;
                        }
                }
                else if (i2cclock && nuclock && nudata && !lastdata) /*Stop bit*/
                {
//                        printf("Stop bit recieved\n");
                        i2cstate=I2C_IDLE;
                        cmosstop();
                }
                else if (i2cclock && nuclock && !nudata && lastdata) /*Start bit*/
                {
//                        printf("Start bit recieved\n");
                        i2cpos=0;
                        cmosstate=CMOS_IDLE;
                }
                break;

                case I2C_ACKNOWLEDGE:
                if (!i2cclock && nuclock)
                {
//                        printf("Acknowledging transfer\n");
                        nudata=0;
                        i2cpos=0;
                        if (i2ctransmit==ARM)
                           i2cstate=I2C_RECIEVE;
                        else
                           i2cstate=I2C_TRANSMIT;
                }
                break;

                case I2C_TRANSACKNOWLEDGE:
                if (!i2cclock && nuclock)
                {
//                        printf("TRANSACKNOWLEDGE %i\n",nudata);
                        if (nudata) /*It's not acknowledged - must be end of transfer*/
                        {
//                                printf("End of transfer\n");
                                i2cstate=I2C_IDLE;
                                cmosstop();
                        }
                        else /*Next byte to transfer*/
                        {
                                i2cstate=I2C_TRANSMIT;
                                cmosnextbyte();
                                i2cpos=0;
//                                rpclog("%02X",i2cbyte);
                        }
                }
                break;

                case I2C_TRANSMIT:
                if (!i2cclock && nuclock)
                {
                        i2cdata=nudata=i2cbyte&128;
                        i2cbyte<<=1;
                        i2cpos++;
                        if (i2cpos==8)
                        {
                                i2cstate=I2C_TRANSACKNOWLEDGE;
//                                printf("Acknowledge mode\n");
                        }
                        i2cclock=nuclock;
                        return;
                }
                break;

        }
        if (!i2cclock && nuclock)
           i2cdata=nudata;
        lastdata=nudata;
        i2cclock=nuclock;
}

void reseti2c(void)
{
        i2cclock=i2cdata=lastdata=0;
        cmosstate=CMOS_IDLE;
        i2ctransmit=ARM;
        cmosaddr=0;
        i2cstate=I2C_IDLE;
        i2cpos=0;
        cmosrw=0;
}
