/*RPCemu v0.6 by Tom Walker
  ROM loader*/
#include <stdint.h>
#include <allegro.h>
#include <stdio.h>
#include "rpcemu.h"
#include "mem.h"

#define MAXROMS 16
static char romfns[MAXROMS+1][256];

// Load the ROM images, call fatal() on error.
void loadroms(void)
{
        FILE *f;
        int finished=0;
        int file=0;
        int c,d,e;
        int len,pos=0;
        struct al_ffblk ff;
        char olddir[512],fn[512];
        char *ext;
	const char *wildcard = "*.*";
	const char *dirname = "roms";

        getcwd(olddir,sizeof(olddir));
        append_filename(fn,exname,dirname,sizeof(fn));
        if (chdir(fn))
	{
		error("Cannot find roms directory %s",dirname);
		abort();
	}
        finished=al_findfirst(wildcard,&ff,0xFFFF&~FA_DIREC);
        if (finished)
	{
		error("Cannot find any file in roms directory '%s' matching '%s'",
		      dirname, wildcard);
		abort();
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
        if (file==0) fatal("Could not load roms from directory '%s'", dirname);
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

//        initpodulerom();
}
