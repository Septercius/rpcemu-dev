/*RPCemu v0.5 by Tom Walker
  'Flexible' ROM loader*/
#include <allegro.h>
#include <stdio.h>
#include "rpcemu.h"

char romfns[17][256];
int loadroms()
{
        FILE *f;
        int finished=0;
        int file=0;
        int c,d,e;
        int len,pos=0;
        struct al_ffblk ff;
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
        for (c=0;c<file;c++)
        {
                f=fopen(romfns[c],"rb");
                fseek(f,-1,SEEK_END);
                len=ftell(f)+1;
                fseek(f,0,SEEK_SET);
                fread(&romb[pos],len,1,f);
                fclose(f);
                pos+=len;
        }
        chdir(olddir);
        return 0;
}
