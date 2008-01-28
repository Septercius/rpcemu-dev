#include <stdio.h>
#include <allegro.h>
#include "rpcemu.h"
#include "podules.h"
#include "podulerom.h"

#define MAXROMS 16
static char romfns[MAXROMS+1][256];

static char *podulerom = NULL;
static int poduleromsize = 0;
static int chunkbase;
static int filebase;

static void makechunk(char type, int filebase, int size)
{
        podulerom[chunkbase++]=type;
        podulerom[chunkbase++]=size&0xFF;
        podulerom[chunkbase++]=(size&0xFF00)>>8;
        podulerom[chunkbase++]=(size&0xFF0000)>>16;

        podulerom[chunkbase++]=(filebase&0x000000FF);
        podulerom[chunkbase++]=(filebase&0x0000FF00)>>8;
        podulerom[chunkbase++]=(filebase&0x00FF0000)>>16;
        podulerom[chunkbase++]=(filebase&0xFF000000)>>24;
}

const char description[] = "RPCEmu additional ROM";


uint8_t readpodulerom(podule *p, int easi, uint32_t addr)
{
        rpclog("READ EASI %08X\n",addr);
        if (easi && (poduleromsize>0))
        {
                addr=(addr&0x00FFFFFF)>>2;
                rpclog("Read %08X\n",podulerom[addr]);
                if (addr<poduleromsize) return podulerom[addr];
                return 0x00;
        }
        return 0xFF;
}

void initpodulerom(void)
{
        int finished=0;
        int file=0;
        struct al_ffblk ff;
        char olddir[512];
        char fn[512];
        int i;

        if (podulerom) free(podulerom);
        poduleromsize = 0;

        getcwd(olddir,sizeof(olddir));
        append_filename(fn,exname,"poduleroms",sizeof(fn));
        if (chdir(fn) == 0) 
        {
                finished=al_findfirst("*.*",&ff,FA_ALL&~FA_DIREC);
                while (!finished && file<MAXROMS)
                {
                        strcpy(romfns[file++],ff.name);
                        finished = al_findnext(&ff);
                }
                al_findclose(&ff);
        }

        chunkbase = 0x10;
        filebase = chunkbase + 8 * file + 8;
        poduleromsize = filebase + ((sizeof(description)+3) &~3);
        podulerom = malloc(poduleromsize);
        if (podulerom == NULL) fatal("Out of Memory");

        memset(podulerom, 0, poduleromsize);
        podulerom[1] = 3; // Interrupt and chunk directories present, byte access

        memcpy(podulerom + filebase, description, sizeof(description));
        makechunk(0xF5, filebase, sizeof(description));
        filebase+=(sizeof(description)+3)&~3;

        for (i=0;i<file;i++)
        {
                FILE *f=fopen(romfns[i],"rb");
                int len;
                if (f==NULL) fatal("Can't open podulerom file\n");
                fseek(f,-1,SEEK_END);
                len = ftell(f) + 1;
                poduleromsize += (len+3)&~3;
                if (poduleromsize > 4096*1024) fatal("Cannot have more than 4MB of podule ROM");
                podulerom = realloc(podulerom, poduleromsize);
                if (podulerom == NULL) fatal("Out of Memory");

                fseek(f,0,SEEK_SET);
                fread(podulerom+filebase,len,1,f);
                fclose(f);
                makechunk(0x81, filebase, len);
                filebase+=(len+3)&~3;
        }
        chdir(olddir);
        addpodule(NULL,NULL,NULL,NULL,NULL,readpodulerom,NULL);
}
