#if defined WIN32 || defined _WIN32 || defined _WIN32
#include <allegro.h>
#include <winalleg.h>
#include <stdio.h>
#include "rpcemu.h"
#include "podules.h"

HINSTANCE hinstLib[8];

static void closedlls(void)
{
        int c;
        for (c=0;c<8;c++)
        {
                if (hinstLib[c]) FreeLibrary(hinstLib[c]);
        }
}

void opendlls(void)
{
        podule tempp;
        struct al_ffblk ff;
        int (*InitDll)();
        int finished;
        int dllnum=0;
        int i;
	char podulesdir[512];
	char searchwildcard[512];
        
        atexit(closedlls);
        for (dllnum=0;dllnum<8;dllnum++) hinstLib[dllnum]=NULL;
        dllnum=0;

	/* Build podules directory path */
	snprintf(podulesdir, sizeof(podulesdir), "%spodules/", rpcemu_get_datadir());

	/* Build a search string */
	snprintf(searchwildcard, sizeof(searchwildcard), "%s*.dll", podulesdir);

        finished = al_findfirst(searchwildcard, &ff, 0xffff & ~FA_DIREC);
        if (finished)
        {
                return;
        }
        while (!finished && dllnum<6)
        {
                char filepath[512];

                snprintf(filepath, sizeof(filepath), "%s%s", podulesdir, ff.name);

                rpclog("podules-win: Loading '%s'\n", filepath);
                hinstLib[dllnum] = LoadLibrary(filepath);
                if (hinstLib[dllnum] == NULL)
                {
                        rpclog("podules-win: Failed to open DLL '%s'\n", filepath);
                        goto nextdll;
                }
                InitDll = (const void *) GetProcAddress(hinstLib[dllnum], "InitDll");
                if (InitDll == NULL)
                {
                        rpclog("podules-win: Couldn't find InitDll in '%s'\n", filepath);
                        goto nextdll;
                }
                InitDll();
                tempp.readb = (const void *) GetProcAddress(hinstLib[dllnum],"readb");
                tempp.readw = (const void *) GetProcAddress(hinstLib[dllnum],"readw");
                tempp.readl = (const void *) GetProcAddress(hinstLib[dllnum],"readl");
                tempp.writeb = (const void *) GetProcAddress(hinstLib[dllnum],"writeb");
                tempp.writew = (const void *) GetProcAddress(hinstLib[dllnum],"writew");
                tempp.writel = (const void *) GetProcAddress(hinstLib[dllnum],"writel");
                tempp.timercallback = (const void *) GetProcAddress(hinstLib[dllnum],"timercallback");
                tempp.reset = (const void *) GetProcAddress(hinstLib[dllnum],"reset");
                i=(GetProcAddress(hinstLib[dllnum],"broken")!=NULL);
                rpclog("Podule is %s\n",(i)?"broken":"normal");
                rpclog("%08X %08X %08X %08X %08X %08X %08X %08X\n",tempp.writel,tempp.writew,tempp.writeb,tempp.readl,tempp.readw,tempp.readb,tempp.timercallback,tempp.reset);
                addpodule(tempp.writel,tempp.writew,tempp.writeb,tempp.readl,tempp.readw,tempp.readb,tempp.timercallback,tempp.reset,i);
                dllnum++;
                
                nextdll:
                finished = al_findnext(&ff);
        }

        al_findclose(&ff);

//        FreeLibrary(hinstLib);
}
#endif

