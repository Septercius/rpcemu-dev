/*RPCemu v0.6 by Tom Walker
  Main loop
  Should be platform independent*/
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <allegro.h>
#include "rpcemu.h"
#include "config.h"
#include "mem.h"
#include "vidc20.h"
#include "keyboard.h"
#include "sound.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"
#include "cmos.h"
#include "superio.h"
#include "romload.h"
#include "cp15.h"
#include "cdrom-iso.h"
#include "podulerom.h"
#include "podules.h"
#include "fdc.h"
#include "hostfs.h"

#ifdef RPCEMU_NETWORKING
#include "network.h"
#endif

unsigned char flaglookup[16][16];

char discname[2][260]={"boot.adf","notboot.adf"};

Config config = {
	CPUModel_ARM7500,	/* model */
	0,			/* rammask */
	0,			/* vrammask */
	0,			/* stretchmode */
	NULL,			/* username */
	NULL,			/* ipaddress */
	NULL,			/* macaddress */
	NULL,			/* bridgename */
	0,			/* refresh */
	1,			/* soundenabled */
	0,			/* skipblits (blit_optimisation) */
	1,			/* cdromenabled */
	0,			/* cdromtype  -- Only used on Windows build */
	"",			/* isoname */
	1,			/* mousehackon */
};

int infocus = 0;
int rinscount = 0;
int cyccount = 0;
int drawscre = 0;
int mousecapture = 0;
int quited = 0;

/* Performance measuring variables */
int updatemips = 0; /**< bool of whether to update the mips speed in the program title bar/other method */
float mips = 0.0f, mhz = 0.0f, tlbsec = 0.0f, flushsec = 0.0f;
uint32_t mipscount;
float mipstotal;

static FILE *arclog; /* Log file handle */

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

/**
 * Write a message to the RPCEmu log file rpclog.txt
 *
 * @param format printf style format of message
 * @param ...    format specific arguments
 */
void
rpclog(const char *format, ...)
{
	va_list arg_list;

	assert(format);

	if (arclog == NULL) {
		arclog = fopen(rpcemu_get_log_path(), "wt");
		if (arclog == NULL) {
			return;
		}
	}

	va_start(arg_list, format);
	vfprintf(arclog, format, arg_list);
	va_end(arg_list);

	fflush(arclog);
}

/**
 * Called once a second to update the performance counters
 */
void domips(void)
{
	mips = (float) inscount / 1000000.0f;
	inscount = 0;

	mipscount += 1;
	mipstotal += mips;

	mhz = (float) cyccount / 1000000.0f;
	cyccount = 0;

	tlbsec = (float) tlbs / 1000000.0f;
	tlbs = 0;

	flushsec = (float) flushes;
	flushes = 0;

	updatemips = 1;
}

/**
 * Reinitialise all emulated subsystems based on current configuration. This
 * is equivalent to resetting the emulated hardware.
 *
 * Called from within the GUI code (Allegro or Windows) when the user has made
 * a change to their preferred configuration, or when the user picks 'Reset'
 * from the menu.
 */
void
resetrpc(void)
{
	rpclog("RPCEmu: Machine reset\n");

        mem_reset(config.rammask + 1);
        resetcp15();
        resetarm();
        resetkeyboard();
        resetiomd();
        reseti2c();
        resetide();
        superio_reset();
        podules_reset();
        podulerom_reset();
        hostfs_reset();

#ifdef RPCEMU_NETWORKING
	network_reset();

	if (config.network_type == NetworkType_EthernetBridging ||
	    config.network_type == NetworkType_IPTunnelling)
	{
		initnetwork();
	}
#endif

	rpclog("RPCEmu: Machine reset complete\n");
}

/**
 * Log additional information about the build and environment.
 */
void
rpcemu_log_information(void)
{
	/* Log version and build type */
	rpclog("RPCEmu " VERSION " [");
#if defined(DYNAREC)
	rpclog("DYNAREC");
#else
	rpclog("INTERPRETER");
#endif

#if defined(_DEBUG)
	rpclog(" DEBUG");
#else
	rpclog(" NO_DEBUG");
#endif
	rpclog("]\n");

	/* Log 32 or 64-bit */
	rpclog("Build: %lu-bit binary\n", (unsigned long) sizeof(void *) * 8);

#if defined __GNUC__ && defined __VERSION__
	rpclog("Compiler: GCC version " __VERSION__ "\n");
#endif

	/* Log details of Operating System */
	rpcemu_log_os();

	/* Log Allegro information */
	rpclog("Allegro version ID: %s\n", allegro_id);
	rpclog("Host Colour Depth: %u\n", desktop_color_depth());
}

/**
 * Set the initial state of all emulated subsystems. Load disc images, CMOS
 * and configuration.
 *
 * Called from each platform's code on program startup.
 *
 * @return Always 0
 */
int
startrpcemu(void)
{
        int c;

	/* On startup log additional information about the build and
	   environment */
	rpcemu_log_information();

        install_keyboard(); /* allegro */
        install_timer();    /* allegro */
        install_mouse();    /* allegro */

 	append_filename(HOSTFS_ROOT, rpcemu_get_datadir(), "hostfs", 511);
        for (c=0;c<511;c++)
        {
                if (HOSTFS_ROOT[c]=='\\')
                   HOSTFS_ROOT[c]='/';
        }
        mem_init();
//printf("Mem inited...\n");
	loadroms();
//printf("ROMs loaded!\n");
        resetarm();
        resetfpa();
        resetiomd();
//printf("IOMD reset!\n");
        resetkeyboard();
//printf("Keyboard reset!\n");
        superio_reset();
//printf("SuperIO reset!\n");
        loadconfig();
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

        sound_init();

        mem_reset(config.rammask + 1);
        initcodeblocks();
        iso_init();
        if (config.cdromtype == 2) /* ISO */
                iso_open(config.isoname);
        podules_reset();
        initpodulerom();
        //initics();
//        iso_open("e:/au_cd8.iso");
//        config.cdromtype = CDROM_ISO;

#ifdef RPCEMU_NETWORKING
	if (config.network_type == NetworkType_EthernetBridging ||
	    config.network_type == NetworkType_IPTunnelling)
	{
		initnetwork();
	}
#endif
        
	/* Call back the mips counting function every second */
	install_int_ex(domips, MSEC_TO_TIMER(1000));

        return 0;
}

/**
 * Execute a chunk of ARM instructions. This is the main entry point for the
 * emulation of the virtual hardware.
 *
 * Called repeatedly from within each platform's main loop.
 */
void
execrpcemu(void)
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
                mouse_poll();
//                sleep(0);
        }
//                sleep(0);
                pollkeyboard();
}

/**
 * Finalise the subsystems, save floppy disc images, CMOS and configuration.
 *
 * Called from each platform's code on program closing.
 */
void
endrpcemu(void)
{
        sound_thread_close();
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

#ifdef RPCEMU_NETWORKING
	network_reset();
#endif
}

/**
 * Load the user's previous chosen configuration. Will fill in sensible
 * defaults if any configuration values are absent.
 *
 * Called on program startup.
 */
static void
loadconfig(void)
{
        char fn[512];
        const char *p;

	append_filename(fn, rpcemu_get_datadir(), "rpc.cfg", 511);
        set_config_file(fn);

	/* Copy the contents of the configfile to the log */
	{
		const char **entries = NULL;
		int n = list_config_entries(NULL, &entries);
		int i;

		for (i = 0; i < n; i++) {
			rpclog("loadconfig: %s = \"%s\"\n", entries[i],
			       get_config_string(NULL, entries[i], "-"));
		}
		free_config_entries(&entries);
	}

        p = get_config_string(NULL,"mem_size",NULL);
        if (!p)                    config.rammask = 0x7FFFFF;
        else if (!strcmp(p,"4"))   config.rammask = 0x1FFFFF;
        else if (!strcmp(p,"8"))   config.rammask = 0x3FFFFF;
        else if (!strcmp(p,"32"))  config.rammask = 0xFFFFFF;
        else if (!strcmp(p,"64"))  config.rammask = 0x1FFFFFF;
        else if (!strcmp(p,"128")) config.rammask = 0x3FFFFFF;
        else                       config.rammask = 0x7FFFFF;

        p = get_config_string(NULL,"vram_size",NULL);
        if (!p) config.vrammask = 0x7FFFFF;
        else if (!strcmp(p,"0"))   config.vrammask = 0;
        else                       config.vrammask = 0x7FFFFF;

        p = get_config_string(NULL,"cpu_type",NULL);
        if (!p) {
                config.model = CPUModel_ARM710;
        } else if (!strcmp(p, "ARM610")) {
                config.model = CPUModel_ARM610;
        } else if (!strcmp(p, "ARM7500")) {
                config.model = CPUModel_ARM7500;
        } else if (!strcmp(p, "ARM7500FE")) {
                config.model = CPUModel_ARM7500FE;
        } else if (!strcmp(p, "ARM810")) {
                config.model = CPUModel_ARM810;
        } else if (!strcmp(p, "SA110")) {
                config.model = CPUModel_SA110;
        } else {
                config.model = CPUModel_ARM710;
        }

        /* ARM7500 (A7000) and ARM7500FE (A7000+) have no VRAM */
        if (config.model == CPUModel_ARM7500 || config.model == CPUModel_ARM7500) {
                config.vrammask = 0;
        }

        config.soundenabled = get_config_int(NULL, "sound_enabled", 1);
        config.stretchmode  = get_config_int(NULL, "stretch_mode",  0);
        config.refresh      = get_config_int(NULL, "refresh_rate", 60);
        config.skipblits    = get_config_int(NULL, "blit_optimisation", 0);
        config.cdromenabled = get_config_int(NULL, "cdrom_enabled", 0);
        config.cdromtype    = get_config_int(NULL, "cdrom_type", 0);

        p = get_config_string(NULL, "cdrom_iso", NULL);
        if (!p) strcpy(config.isoname, "");
        else    strcpy(config.isoname, p);

        config.mousehackon = get_config_int(NULL, "mouse_following", 1);

	p = get_config_string(NULL, "network_type", NULL);
	if (!p) {
		config.network_type = NetworkType_Off;
	} else if (!strcmp(p, "off")) {
		config.network_type = NetworkType_Off;
	} else if (!strcmp(p, "iptunnelling")) {
		config.network_type = NetworkType_IPTunnelling;
	} else if (!strcmp(p, "ethernetbridging")) {
		config.network_type = NetworkType_EthernetBridging;
	} else {
		rpclog("Unknown network_type '%s', defaulting to off\n", p);
		config.network_type = NetworkType_Off;
	}

	/* Take a copy of the string config values, to allow dynamic alteration
	   later */
	p = get_config_string(NULL, "username", NULL);
	if (p) {
		config.username = strdup(p);
	}
	p = get_config_string(NULL, "ipaddress", NULL);
	if (p) {
		config.ipaddress = strdup(p);
	}
	p = get_config_string(NULL, "macaddress", NULL);
	if (p) {
		config.macaddress = strdup(p);
	}
	p = get_config_string(NULL, "bridgename", NULL);
	if (p) {
		config.bridgename = strdup(p);
	}
}

/**
 * Store the user's most recently chosen configuration to disc, for use next
 * time the program starts.
 *
 * Called on program exit.
 */
static void
saveconfig(void)
{
        char s[256];

        sprintf(s, "%i", ((config.rammask + 1) >> 20) << 1);
        set_config_string(NULL,"mem_size",s);
        switch (config.model)
        {
                case CPUModel_ARM610:    sprintf(s, "ARM610"); break;
                case CPUModel_ARM710:    sprintf(s, "ARM710"); break;
                case CPUModel_ARM810:    sprintf(s, "ARM810"); break;
                case CPUModel_SA110:     sprintf(s, "SA110"); break;
                case CPUModel_ARM7500:   sprintf(s, "ARM7500"); break;
                case CPUModel_ARM7500FE: sprintf(s, "ARM7500FE"); break;
                default:
                        /* Forgotten to add a new CPU model to the switch()? */
                        fatal("saveconfig(): unknown cpu model %d\n",
                              config.model);
        }
        set_config_string(NULL,"cpu_type",s);
        if (config.vrammask) set_config_string(NULL, "vram_size", "2");
        else                 set_config_string(NULL, "vram_size", "0");
        set_config_int(NULL, "sound_enabled",     config.soundenabled);
        set_config_int(NULL, "stretch_mode",      config.stretchmode);
        set_config_int(NULL, "refresh_rate",      config.refresh);
        set_config_int(NULL, "blit_optimisation", config.skipblits);
        set_config_int(NULL, "cdrom_enabled",     config.cdromenabled);
        set_config_int(NULL, "cdrom_type",        config.cdromtype);
        set_config_string(NULL, "cdrom_iso",      config.isoname);
        set_config_int(NULL, "mouse_following",   config.mousehackon);

	switch (config.network_type) {
	case NetworkType_Off:              sprintf(s, "off"); break;
	case NetworkType_EthernetBridging: sprintf(s, "ethernetbridging"); break;
	case NetworkType_IPTunnelling:     sprintf(s, "iptunnelling"); break;
	default:
		/* Forgotten to add a new network type to the switch()? */
		fatal("saveconfig(): unknown networktype %d\n",
		      config.network_type);
	}
	set_config_string(NULL, "network_type", s);

	if (config.username) {
		set_config_string(NULL, "username", config.username);
	} else {
		set_config_string(NULL, "username", "");
	}
	if (config.ipaddress) {
		set_config_string(NULL, "ipaddress", config.ipaddress);
	} else {
		set_config_string(NULL, "ipaddress", "");
	}
	if (config.macaddress) {
		set_config_string(NULL, "macaddress", config.macaddress);
	} else {
		set_config_string(NULL, "macaddress", "");
	}
	if (config.bridgename) {
		set_config_string(NULL, "bridgename", config.bridgename);
	} else {
		set_config_string(NULL, "bridgename", "");
	}
}

/**
 * Load an .adf disc image into the specified drive. Save the previous disc
 * image before loading new.
 *
 * @param drive    RPC Drive number, 0 or 1
 * @param filename Full filepath of new .adf to load
 */
void
rpcemu_floppy_load(int drive, const char *filename)
{
	assert(drive == 0 || drive == 1);
	assert(filename);
	assert(*filename);

	saveadf(discname[drive], drive);
	strncpy(discname[drive], filename, 260);
	loadadf(discname[drive], drive);
}
