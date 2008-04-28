#if defined WIN32 || defined _WIN32 || defined _WIN32
#include <allegro.h>
#include <winalleg.h>
#include <stdio.h>
#include "rpcemu.h"
#include "podules.h"

HINSTANCE hinstLib[8];

void closedlls()
{
        int c;
        for (c=0;c<8;c++)
        {
                if (hinstLib[c]) FreeLibrary(hinstLib[c]);
        }
}

void opendlls()
{
        char olddir[512],fn[512];
        podule tempp;
        struct al_ffblk ff;
        int (*InitDll)();
        int finished;
        int dllnum=0;
        int i;
        
        atexit(closedlls);
        for (dllnum=0;dllnum<8;dllnum++) hinstLib[dllnum]=NULL;
        dllnum=0;
        
        getcwd(olddir,sizeof(olddir));
        append_filename(fn,exname,"podules",sizeof(fn));
        if (chdir(fn)) fatal("Cannot find podules directory %s",fn);
        finished=al_findfirst("*.dll",&ff,0xFFFF&~FA_DIREC);
        if (finished)
        {
                chdir(olddir);
                return -1;
        }
        while (!finished && dllnum<6)
        {
                rpclog("Loading %s\n",ff.name);
                hinstLib[dllnum]=LoadLibrary(ff.name);
                if (hinstLib[dllnum] == NULL)
                {
                        rpclog("Failed to open DLL %s\n",ff.name);
                        goto nextdll;
                }
                InitDll = GetProcAddress(hinstLib[dllnum], "InitDll");
                if (InitDll == NULL)
                {
                        rpclog("Couldn't find InitDll in %s\n",ff.name);
                        goto nextdll;
                }
                InitDll();
                tempp.readb=GetProcAddress(hinstLib[dllnum],"readb");
                tempp.readw=GetProcAddress(hinstLib[dllnum],"readw");
                tempp.readl=GetProcAddress(hinstLib[dllnum],"readl");
                tempp.writeb=GetProcAddress(hinstLib[dllnum],"writeb");
                tempp.writew=GetProcAddress(hinstLib[dllnum],"writew");
                tempp.writel=GetProcAddress(hinstLib[dllnum],"writel");
                tempp.timercallback=GetProcAddress(hinstLib[dllnum],"timercallback");
                tempp.reset=GetProcAddress(hinstLib[dllnum],"reset");
                i=(GetProcAddress(hinstLib[dllnum],"broken")!=NULL);
                rpclog("Podule is %s\n",(i)?"broken":"normal");
                rpclog("%08X %08X %08X %08X %08X %08X %08X %08X\n",tempp.writel,tempp.writew,tempp.writeb,tempp.readl,tempp.readw,tempp.readb,tempp.timercallback,tempp.reset);
                addpodule(tempp.writel,tempp.writew,tempp.writeb,tempp.readl,tempp.readw,tempp.readb,tempp.timercallback,tempp.reset,i);
                dllnum++;
                
                nextdll:
                finished = al_findnext(&ff);
        }

        al_findclose(&ff);
        chdir(olddir);
        
//        FreeLibrary(hinstLib);
}
#endif

