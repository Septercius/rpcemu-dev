/*RPCemu v0.6 by Tom Walker
  ROM loader*/
#include <stdint.h>
#include <allegro.h>
#include <stdio.h>
#include "rpcemu.h"
#include "mem.h"

char romfns[17][256];
int loadroms()
{
        FILE *f;
        int finished=0;
        int file=0;
        int c,d,e;
        int len,pos=0;
        struct al_ffblk ff;
		uint32_t temp;
//        char s[256];
        char olddir[512],fn[512];
        char *ext;
        getcwd(olddir,511);
        append_filename(fn,exname,"roms",511);
        chdir(fn);
        finished=al_findfirst("*.*",&ff,0xFFFF&~FA_DIREC);
        if (finished)
        {
                chdir(olddir);
                return -1;
        }
        while (!finished && file<16)
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
//				printf("Loading %f\n",romfns[c]);
                fseek(f,-1,SEEK_END);
                len=ftell(f)+1;
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
        return 0;
}
