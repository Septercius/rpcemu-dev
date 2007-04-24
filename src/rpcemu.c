/*RPCemu v0.6 by Tom Walker
  Main loop
  Should be platform independent*/

#include <allegro.h>
#include "rpcemu.h"

char discname[2][260]={"boot.adf","notboot.adf"};

void loadconfig();
void saveconfig();

int startrpcemu()
{
        int c;
        char *p;
        get_executable_name(exname,511);
        p=get_filename(exname);
        *p=0;
        append_filename(HOSTFS_ROOT,exname,"hostfs",511);
        for (c=0;c<511;c++)
        {
                if (HOSTFS_ROOT[c]=='\\')
                   HOSTFS_ROOT[c]='/';
        }
        initmem();
        if (loadroms())
        {
                error("RiscOS ROMs missing!");
                return -1;
        }
        resetarm();
        resetfpa();
        resetiomd();
        resetkeyboard();
        reset82c711();
        resetide();
        reseti2c();
        loadcmos();
        loadadf("boot.adf",0);
        loadadf("notboot.adf",1);
        initvideo();
        initsound();
        loadconfig();
        reallocmem(rammask+1);
        initcodeblocks();
        return 0;
}

void execrpcemu()
{
        execarm(20000);
        if (drawscre>0)
        {
//                rpclog("Drawscre %i\n",drawscre);
                drawscre--;
                drawscr();
                iomdvsync();
                pollmouse();
                pollkeyboard();
//                sleep(0);
                doosmouse();
//                cmostick();
        }
}

void endrpcemu()
{
        endiomd();
        saveadf(discname[0], 0);
        saveadf(discname[1], 1);
        free(vram);
        free(ram);
        free(ram2);
        free(rom);
        savecmos();
        saveconfig();
        closevideo();
}

void loadconfig()
{
        char fn[512];
        char *p;
        append_filename(fn,exname,"rpc.cfg",511);
        set_config_file(fn);
        p=(char *)get_config_string(NULL,"mem_size",NULL);
        if (!p)                    rammask=0x7FFFFF;
        else if (!strcmp(p,"4"))   rammask=0x1FFFFF;
        else if (!strcmp(p,"8"))   rammask=0x3FFFFF;
        else if (!strcmp(p,"32"))  rammask=0xFFFFFF;
        else if (!strcmp(p,"64"))  rammask=0x1FFFFFF;
        else if (!strcmp(p,"128")) rammask=0x3FFFFFF;
        else                       rammask=0x7FFFFF;
        p=(char *)get_config_string(NULL,"vram_size",NULL);
        if (!p) vrammask=0x1FFFFF;
        else if (!strcmp(p,"0"))        vrammask=0;
        else                       vrammask=0x1FFFFF;
        p=(char *)get_config_string(NULL,"cpu_type",NULL);
        if (!p) model=2;
        else if (!strcmp(p,"ARM610")) model=1;
        else if (!strcmp(p,"ARM7500")) model=0;
        else if (!strcmp(p,"SA110"))   model=3;
        else                           model=2;
        soundenabled=get_config_int(NULL,"sound_enabled",1);
        stretchmode=get_config_int(NULL,"stretch_mode",0);
        refresh=get_config_int(NULL,"refresh_rate",60);
        skipblits=get_config_int(NULL,"blit_optimisation",0);
}

void saveconfig()
{
        char s[256];
        sprintf(s,"%i",((rammask+1)>>20)<<1);
        set_config_string(NULL,"mem_size",s);
        switch (model)
        {
                case 1: sprintf(s,"ARM610"); break;
                case 2: sprintf(s,"ARM710"); break;
                case 3: sprintf(s,"SA110"); break;
                default: sprintf(s,"ARM7500"); break;
        }
        set_config_string(NULL,"cpu_type",s);
        if (vrammask) set_config_string(NULL,"vram_size","2");
        else          set_config_string(NULL,"vram_size","0");
        set_config_int(NULL,"sound_enabled",soundenabled);
        set_config_int(NULL,"stretch_mode",stretchmode);
        set_config_int(NULL,"refresh_rate",refresh);
        set_config_int(NULL,"blit_optimisation",skipblits);
}
