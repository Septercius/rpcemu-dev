#include "rpcemu.h"
#include "podules.h"

/*Podules -
  0 is reserved for extension ROMs
  1 is for additional IDE interface
  2-3 are free
  4-7 are not implemented (yet)*/
podule podules[8];
int freepodule;

void initpodules()
{
        int c;
        for (c=0;c<8;c++)
            podules[c].readw=podules[c].readb=podules[c].writew=podules[c].writeb=NULL;
        freepodule=1;
}
  
int addpodule(void (*writew)(struct podule *p, unsigned short addr, unsigned short val),
              void (*writeb)(struct podule *p, unsigned short addr, unsigned char val),
              unsigned short (*readw)(struct podule *p, unsigned short addr),
              unsigned char  (*readb)(struct podule *p, unsigned short addr))
{
        if (freepodule==8) return -1; /*All podules in use!*/
        podules[freepodule].readw=readw;
        podules[freepodule].readb=readb;
        podules[freepodule].writew=writew;
        podules[freepodule].writeb=writeb;
        rpclog("Podule added at %i\n",freepodule);
        freepodule++;
}

void writepodulew(int num, unsigned short addr, unsigned short val)
{
        if (podules[num].writew)
           podules[num].writew(&podules[num],addr,val);
}

void writepoduleb(int num, unsigned short addr, unsigned char val)
{
        if (podules[num].writeb)
           podules[num].writeb(&podules[num],addr,val);
}

unsigned short readpodulew(int num, unsigned short addr)
{
        if (podules[num].readw)
           return podules[num].readw(&podules[num],addr);
        return 0xFFFF;
}

unsigned char readpoduleb(int num, unsigned short addr)
{
        if (podules[num].readb)
           return podules[num].readb(&podules[num],addr);
        return 0xFF;
}
