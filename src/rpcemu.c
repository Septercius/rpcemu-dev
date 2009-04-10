/*RPCemu v0.6 by Tom Walker
  Main loop
  Should be platform independent*/
#include <assert.h>
#include <stdint.h>
#include <allegro.h>
#include "rpcemu.h"
#include "mem.h"
#include "vidc20.h"
#include "keyboard.h"
#include "sound.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"
#include "cmos.h"
#include "82c711.h"
#include "romload.h"
#include "cp15.h"
#include "cdrom-iso.h"
#include "podulerom.h"
#include "podules.h"

int cdromtype;
unsigned char flaglookup[16][16];

char discname[2][260]={"boot.adf","notboot.adf"};
char exname[512] = {0};

int vrammask = 0;
CPUModel model = CPUModel_ARM7500;
int rammask = 0;
int stretchmode = 0;
int infocus = 0;
int refresh = 0;
int skipblits = 0;
int rinscount = 0;
int cyccount = 0;
int timetolive = 0;
int drawscre = 0;
int mousecapture = 0;
int quited = 0;
const char *username = NULL;
const char *ipaddress = NULL;

static void loadconfig(void);
static void saveconfig(void);

#ifdef _DEBUG
/**
 * UNIMPLEMENTEDFL
 *
 * Used to report sections of code that have not been implemented yet.
 * Do not use this function directly. Use the macro UNIMPLEMENTED() instead.
 *
 * @param file    File function is called from
 * @param line    Line function is called from
 * @param section Section code is missing from eg. "IOMD register" or
 *                "HostFS filecore message"
 * @param format  Section specific information
 * @param ...     Section specific information variable arguments
 */
void UNIMPLEMENTEDFL(const char *file, unsigned line, const char *section,
                     const char *format, ...)
{
	char buffer[1024];
	va_list arg_list;

	assert(file);
	assert(section);
	assert(format);

	va_start(arg_list, format);
	vsprintf(buffer, format, arg_list);
	va_end(arg_list);

	rpclog("UNIMPLEMENTED: %s: %s(%u): %s\n",
	       section, file, line, buffer);

	fprintf(stderr,
	        "UNIMPLEMENTED: %s: %s(%u): %s\n",
	        section, file, line, buffer);
}
#endif /* _DEBUG */

void resetrpc()
{
        reallocmem(rammask + 1);
        resetcp15();
        resetarm();
        resetkeyboard();
        resetiomd();
        reseti2c();
        resetide();
        reset82c711();
        resetpodules();
}

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
//printf("Mem inited...\n");
	loadroms();
//printf("ROMs loaded!\n");
        resetarm();
        resetfpa();
        resetiomd();
//printf("IOMD reset!\n");
        resetkeyboard();
//printf("Keyboard reset!\n");
        reset82c711();
//printf("82c711 reset!\n");
        resetide();
//printf("IDE reset!\n");
        reseti2c();
//printf("i2C reset!\n");
        loadcmos();
        loadadf("boot.adf",0);
        loadadf("notboot.adf",1);
//printf("About to init video...\n");
        initvideo();
//printf("Video inited!\n");
        loadconfig();
        initsound();
        reallocmem(rammask+1);
        initcodeblocks();
        iso_init();
        if (cdromtype==2) /*ISO*/
           iso_open(isoname);
        initpodules();
        initpodulerom();
        //initics();
//        iso_open("e:/au_cd8.iso");
//        cdromtype=CDROM_ISO;
        return 0;
}

void execrpcemu()
{
//	static int c;
//	printf("Exec %i\n",c);
//c++;
        execarm(20000);
        drawscr(drawscre);
        if (drawscre>0)
        {
//                rpclog("Drawscre %i\n",drawscre);
                drawscre--;
                if (drawscre>5) drawscre=0;
                
//				poll_keyboard();
//				poll_mouse();
                pollmouse();
//                sleep(0);
                doosmouse();
        }
//                pollmouse();
//                sleep(0);
//                doosmouse();
                pollkeyboard();
}

void endrpcemu()
{
        closevideo();
        endiomd();
        saveadf(discname[0], 0);
        saveadf(discname[1], 1);
        free(vram);
        free(ram);
        free(ram2);
        free(rom);
        savecmos();
        saveconfig();
}

static void loadconfig(void)
{
        char fn[512];
        const char *p;

        append_filename(fn,exname,"rpc.cfg",511);
        set_config_file(fn);
        p = get_config_string(NULL,"mem_size",NULL);
        if (!p)                    rammask=0x7FFFFF;
        else if (!strcmp(p,"4"))   rammask=0x1FFFFF;
        else if (!strcmp(p,"8"))   rammask=0x3FFFFF;
        else if (!strcmp(p,"32"))  rammask=0xFFFFFF;
        else if (!strcmp(p,"64"))  rammask=0x1FFFFFF;
        else if (!strcmp(p,"128")) rammask=0x3FFFFFF;
        else                       rammask=0x7FFFFF;
        #ifdef DYNAREC
        model = CPUModel_SA110;
        vrammask=0x7FFFFF; /*2mb VRAM*/
        #else
        p = get_config_string(NULL,"vram_size",NULL);
        if (!p) vrammask=0x7FFFFF;
        else if (!strcmp(p,"0"))        vrammask=0;
        else                       vrammask=0x7FFFFF;
        p = get_config_string(NULL,"cpu_type",NULL);
        if (!p) model = CPUModel_ARM710;
        else if (!strcmp(p, "ARM610"))  model = CPUModel_ARM610;
        else if (!strcmp(p, "ARM7500")) model = CPUModel_ARM7500;
        else if (!strcmp(p, "SA110"))   model = CPUModel_SA110;
        else                            model = CPUModel_ARM710;
        #endif
        soundenabled=get_config_int(NULL,"sound_enabled",1);
        stretchmode=get_config_int(NULL,"stretch_mode",0);
        refresh=get_config_int(NULL,"refresh_rate",60);
        skipblits=get_config_int(NULL,"blit_optimisation",0);
        cdromenabled=get_config_int(NULL,"cdrom_enabled",0);
        cdromtype=get_config_int(NULL,"cdrom_type",0);
        p = get_config_string(NULL,"cdrom_iso",NULL);
        if (!p) strcpy(isoname,"");
        else    strcpy(isoname,p);
        mousehackon=get_config_int(NULL,"mouse_following",1);
        username=get_config_string(NULL,"username",NULL);
        ipaddress=get_config_string(NULL,"ipaddress",NULL);
}

static void saveconfig(void)
{
        char s[256];
        sprintf(s,"%i",((rammask+1)>>20)<<1);
        set_config_string(NULL,"mem_size",s);
        #ifndef DYNAREC
        switch (model)
        {
                case CPUModel_ARM610:  sprintf(s, "ARM610"); break;
                case CPUModel_ARM710:  sprintf(s, "ARM710"); break;
                case CPUModel_SA110:   sprintf(s, "SA110"); break;
                case CPUModel_ARM7500: sprintf(s, "ARM7500"); break;
                default: fprintf(stderr, "saveconfig(): unknown cpu model %d\n", model); break;
        }
        set_config_string(NULL,"cpu_type",s);
        if (vrammask) set_config_string(NULL,"vram_size","2");
        else          set_config_string(NULL,"vram_size","0");
        #endif
        set_config_int(NULL,"sound_enabled",soundenabled);
        set_config_int(NULL,"stretch_mode",stretchmode);
        set_config_int(NULL,"refresh_rate",refresh);
        set_config_int(NULL,"blit_optimisation",skipblits);
        set_config_int(NULL,"cdrom_enabled",cdromenabled);
        set_config_int(NULL,"cdrom_type",cdromtype);
        set_config_string(NULL,"cdrom_iso",isoname);
        set_config_int(NULL,"mouse_following",mousehackon);
}
