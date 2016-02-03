/*RPCemu v0.6 by Tom Walker
  Main loop
  Should be platform independent*/
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <allegro.h>
#if defined WIN32 || defined _WIN32
#include <winalleg.h>
#endif

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
#include "superio.h"
#include "i8042.h"
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

char discname[2][260]={"boot.adf","notboot.adf"};

Machine machine; /**< The details of the current machine being emulated */

/** Array of details of models the emulator can emulate, must be kept in sync with
    Model enum in rpcemu.h */
const Model_Details models[] = {
	{ "Risc PC - ARM610",                "RPC610", CPUModel_ARM610,    IOMDType_IOMD,      SuperIOType_FDC37C665GT, I2C_PCF8583 },
	{ "Risc PC - ARM710",                "RPC710", CPUModel_ARM710,    IOMDType_IOMD,      SuperIOType_FDC37C665GT, I2C_PCF8583 },
	{ "Risc PC - StrongARM",             "RPCSA",  CPUModel_SA110,     IOMDType_IOMD,      SuperIOType_FDC37C665GT, I2C_PCF8583 },
	{ "A7000",                           "A7000",  CPUModel_ARM7500,   IOMDType_ARM7500,   SuperIOType_FDC37C665GT, I2C_PCF8583 },
	{ "A7000+ (experimental)",           "A7000+", CPUModel_ARM7500FE, IOMDType_ARM7500FE, SuperIOType_FDC37C665GT, I2C_PCF8583 },
	{ "Risc PC - ARM810 (experimental)", "RPC810", CPUModel_ARM810,    IOMDType_IOMD,      SuperIOType_FDC37C665GT, I2C_PCF8583 },
	{ "Phoebe (RPC2)",                   "Phoebe", CPUModel_SA110,     IOMDType_IOMD2,     SuperIOType_FDC37C672,   I2C_PCF8583 | I2C_SPD_DIMM0 }
};

Config config = {
	0,			/* mem_size */
	0,			/* vrammask */
	NULL,			/* username */
	NULL,			/* ipaddress */
	NULL,			/* macaddress */
	NULL,			/* bridgename */
	0,			/* refresh */
	1,			/* soundenabled */
	1,			/* cdromenabled */
	0,			/* cdromtype  -- Only used on Windows build */
	"",			/* isoname */
	1,			/* mousehackon */
	0,			/* mousetwobutton */
	NetworkType_Off,	/* network_type */
	0,			/* cpu_idle */
};

/* Performance measuring variables */
int updatemips = 0; /**< bool of whether to update the mips speed in the program title bar */
Perf perf = {
	0.0f, /* mips */
	0.0f, /* mhz */
	0.0f, /* tlb_sec */
	0.0f, /* flush_sec */
	0,    /* mips_count */
	0.0f  /* mips_total */
};

int infocus = 0;
int cyccount = 0;
int drawscre = 0;
int mousecapture = 0;
int quited = 0;

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
static void
domips(void)
{
	perf.mips = (float) inscount / 1000000.0f;
	inscount = 0;

	perf.mips_count += 1;
	perf.mips_total += perf.mips;

	perf.mhz = (float) cyccount / 1000000.0f;
	cyccount = 0;

	perf.tlb_sec = (float) tlbs / 1000000.0f;
	tlbs = 0;

	perf.flush_sec = (float) flushes;
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

        mem_reset(config.mem_size);
        cp15_reset(machine.cpu_model);
        resetarm(machine.cpu_model);
        keyboard_reset();
	iomd_reset(machine.iomd_type);

        reseti2c(machine.i2c_devices);
        resetide();
        superio_reset(machine.super_type);
	i8042_reset();
        podules_reset();
        podulerom_reset(); // must be called after podules_reset()
        hostfs_reset();

#ifdef RPCEMU_NETWORKING
	network_reset();

	if (config.network_type == NetworkType_EthernetBridging ||
	    config.network_type == NetworkType_IPTunnelling)
	{
		network_init();
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
	char cwd[1024];
	int width, height;

	/* Log version and build type */
	rpclog("RPCEmu " VERSION " [");
	if (arm_is_dynarec()) {
		rpclog("DYNAREC");
	} else {
		rpclog("INTERPRETER");
	}

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

	/* Log display information */
	if (get_desktop_resolution(&width, &height) == 0) {
		rpclog("Desktop Resolution: %d x %d\n", width, height);
	}
	rpclog("Host Colour Depth: %u\n", desktop_color_depth());

	/* Log working directory */
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		rpclog("Working Directory: %s\n", cwd);
	}
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
	/* On startup log additional information about the build and
	   environment */
	rpcemu_log_information();

        install_keyboard(); /* allegro */
        install_timer();    /* allegro */
        install_mouse();    /* allegro */

	loadconfig();
	hostfs_init();
	mem_init();
	cp15_init();
	arm_init();
	loadroms();
        loadcmos();
        fdc_adf_load("boot.adf",0);
        fdc_adf_load("notboot.adf",1);
        initvideo();

        sound_init();

        initcodeblocks();
        iso_init();
        if (config.cdromtype == 2) /* ISO */
                iso_open(config.isoname);
        initpodulerom();

	/* Other components are initialised in the same way as the hardware
	   being reset */
	resetrpc();

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
                
                mouse_poll();
        }

	keyboard_poll();
}

/**
 * Attempt to reduce CPU usage by checking for pending interrupts, running
 * any callbacks, and then sleeping for a short period of time.
 *
 * Called when RISC OS calls "Portable_Idle" SWI.
 */
void
rpcemu_idle(void)
{
	int hostupdate = 0;

	/* Loop while no interrupts pending */
	while (!armirq) {
		/* Run down any callback timers */
		if (kcallback) {
			kcallback--;
			if (kcallback <= 0) {
				kcallback = 0;
				keyboard_callback_rpcemu();
			}
		}
		if (mcallback) {
			mcallback -= 10;
			if (mcallback <= 0) {
				mcallback = 0;
				mouse_ps2_callback();
			}
		}
		if (fdccallback) {
			fdccallback -= 10;
			if (fdccallback <= 0) {
				fdccallback = 0;
				fdc_callback();
			}
		}
		if (idecallback) {
			idecallback -= 10;
			if (idecallback <= 0) {
				idecallback = 0;
				callbackide();
			}
		}
		if (motoron) {
			/* Not much point putting a counter here */
			iomd.irqa.status |= IOMD_IRQA_FLOPPY_INDEX;
			updateirqs();
		}
		/* Sleep if no interrupts pending */
		if (!armirq) {
#ifdef RPCEMU_WIN
			Sleep(1);
#else
			struct timespec tm;

			tm.tv_sec = 0;
			tm.tv_nsec = 1000000;
			nanosleep(&tm, NULL);
#endif
		}
		/* Run other periodic actions */
		if (!armirq && !(++hostupdate > 20)) {
			hostupdate = 0;
			drawscr(drawscre);
			if (drawscre > 0) {
				drawscre--;
				if (drawscre > 5)
					drawscre = 0;

				mouse_poll();
			}
			keyboard_poll();
		}
	}
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
        iomd_end();
        fdc_adf_save(discname[0], 0);
        fdc_adf_save(discname[1], 1);
        free(vram);
        free(ram00);
        free(ram01);
        free(rom);
        savecmos();
        saveconfig();

#ifdef RPCEMU_NETWORKING
	network_reset();
#endif
}

/**
 * Called whenever the user's chosen model is changed
 *
 * Caches details of the model in the machine struct
 *
 * @param model New model being selected
 */
void
rpcemu_model_changed(Model model)
{
	/* Cache details from the models[] array into the machine struct for speed of lookup */
	machine.model       = model;
	machine.cpu_model   = models[model].cpu_model;
	machine.iomd_type   = models[model].iomd_type;
	machine.super_type  = models[model].super_type;
	machine.i2c_devices = models[model].i2c_devices;
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
	Model model;
	int i;

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

	p = get_config_string(NULL, "mem_size", NULL);
	if (p == NULL) {
		config.mem_size = 16;
	} else if (!strcmp(p, "4")) {
		config.mem_size = 4;
	} else if (!strcmp(p, "8")) {
		config.mem_size = 8;
	} else if (!strcmp(p, "32")) {
		config.mem_size = 32;
	} else if (!strcmp(p, "64")) {
		config.mem_size = 64;
	} else if (!strcmp(p, "128")) {
		config.mem_size = 128;
	} else if (!strcmp(p, "256")) {
		config.mem_size = 256;
	} else {
		config.mem_size = 16;
	}

        p = get_config_string(NULL,"vram_size",NULL);
        if (!p) config.vrammask = 0x7FFFFF;
        else if (!strcmp(p,"0"))   config.vrammask = 0;
        else                       config.vrammask = 0x7FFFFF;

	p = get_config_string(NULL, "model", NULL);
	model = Model_RPCARM710;
	if (p != NULL) {
		for (i = 0; i < Model_MAX; i++) {
			if (stricmp(p, models[i].name_config) == 0) {
				model = i;
				break;
			}
		}
	}
	rpcemu_model_changed(model);

	/* A7000 and A7000+ have no VRAM */
	if (model == Model_A7000 || model == Model_A7000plus) {
		config.vrammask = 0;
	}

	/* If Phoebe, override some settings */
	if (model == Model_Phoebe) {
		config.mem_size = 256;
		config.vrammask = 0x3fffff;
	}

        config.soundenabled = get_config_int(NULL, "sound_enabled", 1);
        config.refresh      = get_config_int(NULL, "refresh_rate", 60);
        config.cdromenabled = get_config_int(NULL, "cdrom_enabled", 0);
        config.cdromtype    = get_config_int(NULL, "cdrom_type", 0);

        p = get_config_string(NULL, "cdrom_iso", NULL);
        if (!p) strcpy(config.isoname, "");
        else    strcpy(config.isoname, p);

        config.mousehackon    = get_config_int(NULL, "mouse_following", 1);
        config.mousetwobutton = get_config_int(NULL, "mouse_twobutton", 0);

	p = get_config_string(NULL, "network_type", NULL);
	if (!p) {
		config.network_type = NetworkType_Off;
	} else if (!stricmp(p, "off")) {
		config.network_type = NetworkType_Off;
	} else if (!stricmp(p, "iptunnelling")) {
		config.network_type = NetworkType_IPTunnelling;
	} else if (!stricmp(p, "ethernetbridging")) {
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

	config.cpu_idle = get_config_int(NULL, "cpu_idle", 0);
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

	sprintf(s, "%u", config.mem_size);
	set_config_string(NULL, "mem_size", s);
	sprintf(s, "%s", models[machine.model].name_config);
	set_config_string(NULL, "model", s);
        if (config.vrammask) set_config_string(NULL, "vram_size", "2");
        else                 set_config_string(NULL, "vram_size", "0");
        set_config_int(NULL, "sound_enabled",     config.soundenabled);
        set_config_int(NULL, "refresh_rate",      config.refresh);
        set_config_int(NULL, "cdrom_enabled",     config.cdromenabled);
        set_config_int(NULL, "cdrom_type",        config.cdromtype);
        set_config_string(NULL, "cdrom_iso",      config.isoname);
        set_config_int(NULL, "mouse_following",   config.mousehackon);
        set_config_int(NULL, "mouse_twobutton",   config.mousetwobutton);

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

	set_config_int(NULL, "cpu_idle", config.cpu_idle);
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

	fdc_adf_save(discname[drive], drive);
	strncpy(discname[drive], filename, 260);
	fdc_adf_load(discname[drive], drive);
}
