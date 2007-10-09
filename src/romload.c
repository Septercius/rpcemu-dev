/*RPCemu v0.6 by Tom Walker
  ROM loader*/
#include <stdint.h>
#include <allegro.h>
#include <stdio.h>
#include "rpcemu.h"
#include "mem.h"

#define MAXROMS 16
static char romfns[MAXROMS+1][256];

char *podulerom = NULL;
int poduleromsize = 0;
int chunkbase;
int filebase;

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

static void initpodulerom(void)
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
        if (chdir(fn)) return;
        finished=al_findfirst("*.*",&ff,FA_ALL&~FA_DIREC);
        while (!finished && file<MAXROMS)
        {
                strcpy(romfns[file++],ff.name);
                finished = al_findnext(&ff);
        }
        al_findclose(&ff);
        if (file==0) 
        {
                chdir(olddir);
                return;
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
}

uint32_t readeasi(uint32_t addr)
{
        // Only podule 0 is used
        if ((poduleromsize>0) && (addr&0x1F000000)==0x08000000) 
        {
                addr=(addr&0x00FFFFFF)>>2;
                if (addr<poduleromsize) return podulerom[addr];
                return 0x0000000;
        }

        // Other podules not present
        return 0xFFFFFFFF;
}


int loadroms(void)
{
        FILE *f;
        int finished=0;
        int file=0;
        int c,d,e;
        int len,pos=0;
        struct al_ffblk ff;
        char olddir[512],fn[512];
        char *ext;

        getcwd(olddir,sizeof(olddir));
        append_filename(fn,exname,"roms",sizeof(fn));
        if (chdir(fn)) fatal("Cannot find roms directory %s",fn);
        finished=al_findfirst("*.*",&ff,0xFFFF&~FA_DIREC);
        if (finished)
        {
                chdir(olddir);
                return -1;
        }
        while (!finished && file<MAXROMS)
        {
                ext=get_extension(ff.name);
//				printf("Found rom %s\n",ff.name);
                if (stricmp(ext,"txt"))
                {
                        strcpy(romfns[file],ff.name);
                        file++;
                }
                finished = al_findnext(&ff);
        }
        al_findclose(&ff);
        if (file==0)
        {
                chdir(olddir);
                return -1;
        }
//printf("Loading file...\n");
        for (c=0;c<file;c++)
        {
                for (d=0;d<file;d++)
                {
                        if (c>d)
                        {
                                e=0;
                                while (romfns[c][e]==romfns[d][e] && romfns[c][e])
                                      e++;
                                if (romfns[c][e]<romfns[d][e])
                                {
                                        memcpy(romfns[16],romfns[c],256);
                                        memcpy(romfns[c],romfns[d],256);
                                        memcpy(romfns[d],romfns[16],256);
                                }
                        }
                }
        }
//printf("Really loading files...\n");
        for (c=0;c<file;c++)
        {
                f=fopen(romfns[c],"rb");
                //printf("Loading %s\n",romfns[c]);
                if (f==NULL) fatal("Can't open rom file %s",romfns[c]);
                fseek(f,-1,SEEK_END);
                len=ftell(f)+1;
                if (pos + len > ROMSIZE) fatal("ROM files larger than 8MB");
//printf("Reading %i bytes\n",len);
                fseek(f,0,SEEK_SET);
                fread(&romb[pos],len,1,f);
                fclose(f);
                pos+=len;
        }
        chdir(olddir);
#ifdef _RPCEMU_BIG_ENDIAN /*Byte swap*/
#error It's defined...
//printf("Byte swapping...\n");
		for (c=0;c<0x800000;c+=4)
		{
                                uint32_t temp;
				temp=rom[c>>2];
				temp=((temp&0xFF000000)>>24)|((temp&0x00FF0000)>>8)|((temp&0x0000FF00)<<8)|((temp&0x000000FF)<<24);
//				temp=((temp>>24)&0xFF)|((temp>>8)&0xFF00)|((temp<<8)&0xFF0000)|((temp<<24)|0xFF000000);
				rom[c>>2]=temp;
		}
#endif
        /*Patch ROM for 8 meg vram!*/
        if (rom[0x14820>>2]==0xE3560001 && /*Check for ROS 4.02 startup*/
            rom[0x14824>>2]==0x33A02050 &&
            rom[0x14828>>2]==0x03A02004 &&
            rom[0x1482C>>2]==0x83A02008)
           rom[0x14824>>2]=0xE3A06008; /*MOV R6,#8 - 8 megs*/

        initpodulerom();

        return 0;
}
