#include <stdio.h>
#include "rpcemu.h"
#include "podules.h"
#include "ide.h"

unsigned char icsrom[8192];
int icspage;

unsigned char icsreadb(podule *p, unsigned short addr)
{
        int temp;
//        rpclog("Read ICSB %04X\n",addr);
        switch (addr&0x3000)
        {
                case 0x0000: case 0x1000:
                temp=((addr&0x1FFC)|(icspage<<13))>>2;
                return icsrom[temp];
                case 0x3000:
                ideboard=2;
                return readide(((addr>>2)&7)+0x1F0);
        }
}

unsigned short icsreadw(podule *p, unsigned short addr)
{
        if ((addr&0x3000)==0x3000)
        {
//                rpclog("Read IDEW\n");
                ideboard=2;
                return readidew();
        }
        return icsreadb(p,addr);
}

void icswriteb(podule *p, unsigned long addr, unsigned char val)
{
//        rpclog("Write ICSB %04X %02X\n",addr,val);
        switch (addr&0x3000)
        {
                case 0x2000: icspage=val; return;
                case 0x3000:
                ideboard=2;
                writeide(((addr>>2)&7)+0x1F0,val);
                return;
        }
}

void icswritew(podule *p, unsigned long addr, unsigned short val)
{
        if ((addr&0x3000)==0x3000)
        {
//                rpclog("WRITEIDEW\n");
                ideboard=2;
                return writeidew(val);
        }
        icswriteb(p,addr,val);
}

void initics()
{
        FILE *f;
        char fn[512];
        append_filename(fn,exname,"zidefs",sizeof(fn));
        f=fopen(fn,"rb");
        if (!f)
        {
                rpclog("Failed to open ZIDEFS!\n");
                return;
        }
        fread(icsrom,8192,1,f);
        fclose(f);
        addpodule(NULL,icswritew,icswriteb,NULL,icsreadw,icsreadb,NULL);
//        rpclog("ICS Initialised!\n");
}
