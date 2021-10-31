/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2005-2010 Sarah Walker

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Main loop
   Should be platform independent */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#if defined WIN32 || defined _WIN32
#undef UNICODE
#include <windows.h>
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
#include "disc.h"
#include "disc_adf.h"
#include "disc_hfe.h"
#include "disc_mfm_common.h"

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
	0,			/* vram_size */
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
	1,			/* show_fullscreen_message */
	NULL,			/* network_capture */
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

PortForwardRule port_forward_rules[MAX_PORT_FORWARDS]; ///< Port forward rules accross the NAT

int drawscre = 0;
int quited = 0;

static FILE *arclog; /* Log file handle */

static int cycles;

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

        mem_reset(config.mem_size, config.vram_size);
        cp15_reset(machine.cpu_model);
	arm_reset(machine.cpu_model);
        keyboard_reset();
	iomd_reset(machine.iomd_type);

        reseti2c(machine.i2c_devices);
        resetide();
        superio_reset(machine.super_type);
	i8042_reset();
	cmos_reset();
        podules_reset();
        podulerom_reset(); // must be called after podules_reset()
        hostfs_reset();

#ifdef RPCEMU_NETWORKING
	network_reset();

	if (config.network_type != NetworkType_Off) {
		network_init();
	}
#endif

	cycles = 0;

	rpclog("RPCEmu: Machine reset complete\n");
}

/**
 * Log additional information about the build and environment.
 */
void
rpcemu_log_information(void)
{
	char cwd[1024];
	time_t now;
	char buffer[22];
	struct tm* tm_info;

	/* Time and date of this run */
	time(&now);
	tm_info = localtime(&now);
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
	rpclog("localtime: %s\n", buffer);
	tm_info = gmtime(&now);
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
	rpclog("   gmtime: %s\n", buffer);

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

	/* Log Compiler */
	/* Clang must be tested before GCC because Clang also defines __GNUC__ */
#if defined __clang__ && defined __VERSION__
	rpclog("Compiler: Clang version " __VERSION__ "\n");
#elif defined __GNUC__ && defined __VERSION__
	rpclog("Compiler: GCC version " __VERSION__ "\n");
#endif
	/* Log details of Operating System */
	rpcemu_log_os();

	/* Log details of Platform (qt) */
	rpcemu_log_platform();

	/* Log working directory */
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		rpclog("Working Directory: %s\n", cwd);
	}
}

/**
 * Start enough of the emulator system to allow
 * the GUI to initialise (e.g. load the config to init
 * the configure window)
 *
 * Called from each platform's code on program startup.
 */
void
rpcemu_prestart(void)
{
	/* On startup log additional information about the build and environment */
	rpcemu_log_information();

	config_load(&config);
}

/**
 * Set the initial state of all emulated subsystems. Load disc images, CMOS
 * and configuration.
 *
 * Called from each platform's code on program startup.
 *
 * @return Always 0
 */
void
rpcemu_start(void)
{
	hostfs_init();
	mem_init();
	cp15_init();
	arm_init();
	loadroms();
        cmos_init();
        fdc_init();
        adf_init();
        hfe_init();
        mfm_init();
        fdc_image_load("boot.adf", 0);
        fdc_image_load("notboot.adf", 1);
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
	cycles += 20000;

	while (cycles > 0) {
		cycles -= arm_exec();

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
			fdccallback -= 100;
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
			disc_poll();
		}
	}

	if (drawscre > 0) {
		drawscr();
		drawscre--;
		if (drawscre > 5) {
			drawscre = 0;
		}
	}
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
	/* Loop while no interrupts pending */
	while (!arm.event) {
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
			fdccallback -= 100;
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
		if (!arm.event) {
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
		if (!arm.event) {
			if (drawscre > 0) {
				drawscr();
				drawscre--;
				if (drawscre > 5) {
					drawscre = 0;
				}
			}
			rpcemu_idle_process_events();
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
        fdc_image_save(discname[0], 0);
        fdc_image_save(discname[1], 1);
        free(vram);
        free(ram00);
        free(ram01);
        free(rom);
        savecmos();
        config_save(&config);

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

	fdc_image_save(discname[drive], drive);

	if (strlen(filename) > sizeof(discname[drive]) - 1) {
		// New disc image path too long
		error("Disc image disc path \'%s\' too long", filename);
	} else {
		strcpy(discname[drive], filename);
		fdc_image_load(discname[drive], drive);
	}
}

/**
 * Find a filename's extension (bit after the .)
 *
 * @param filename string to check
 * @returns pointer to first char in extension, or pointer to
 *          null terminator (empty string) if no extension found
 */
const char *
rpcemu_file_get_extension(const char *filename)
{
	const char *position;

	assert(filename);

	position = strrchr(filename, '.');
	if (position == NULL) {
		/* No extension, return empty string */
		return &filename[strlen(filename)];
	} else {
		/* Found extension */
		return position + 1;
	}
}

/**
 * Test whether the changes in configuration would require an emulated
 * machine reset
 * 
 * Called from GUI thread, is thread safe due to only reading the emulator
 * state
 * 
 * @thread GUI
 * @param new_config New configuration values
 * @param new_model New configuration values
 * @returns Bool of whether emulated machine reset required
 */
int
rpcemu_config_is_reset_required(const Config *new_config, Model new_model)
{
	int needs_reset = 0;
	assert(new_config);

	if(machine.model != new_model) {
		needs_reset = 1;
	}

	if(config.mem_size != new_config->mem_size) {
		needs_reset = 1;
	}

	/* vram size has changed on a machine without fixed vram size */
	if (config.vram_size != new_config->vram_size
	   && (machine.model != Model_A7000 &&
	       machine.model != Model_A7000plus &&
	       machine.model != Model_Phoebe))
	{
		needs_reset = 1;
	}

	if (config.network_type != new_config->network_type) {
		needs_reset = 1;
	}

	// TODO Various network, MAC/IP/bridgename changes will also cause reset

	return needs_reset;
}

/**
 * Apply a new configuration and reset the emulator is required
 * 
 * @thread emulator
 * @param new_config the new configuration
 * @param new_model the new configuration
 */
void
rpcemu_config_apply_new_settings(Config *new_config, Model new_model)
{
	int needs_reset = 0;
	int sound_changed = 0;

	/* Sound state changed? */
	if((config.soundenabled && !new_config->soundenabled)
	   || (new_config->soundenabled && !config.soundenabled))
	{
		sound_changed = 1;
	}

	/* Changed machine we're emulating? */
	if(new_model != machine.model) {
		rpcemu_model_changed(new_model);
		needs_reset = 1;
	}

	/* If an A7000 or an A7000+ it does not have vram */
	if (machine.model == Model_A7000 || machine.model == Model_A7000plus) {
		new_config->vram_size = 0;
	}

	/* If Phoebe, override some settings */
	if (machine.model == Model_Phoebe) {
		new_config->mem_size = 256;
		new_config->vram_size = 4;
	}

	if (new_config->mem_size != config.mem_size) {
		needs_reset = 1;
	}

	if (new_config->vram_size != config.vram_size) {
		needs_reset = 1;
	}

	/* Copy new settings over */
	memcpy(&config, new_config, sizeof(Config));

	// Save the settings to the rpc.cfg file
	config_save(&config);

	if(sound_changed) {
		if(config.soundenabled) {
			sound_restart();
		} else {
			sound_pause();
		}
	}

	/* Reset the machine after the config variables have been set to their
	   new values */
	if(needs_reset) {
		resetrpc();
	}
}

/**
 * Add a forwarding rule to the NAT
 *
 * @param type      TCP or UDP
 * @param emu_port  port number on emulated machine
 * @param host_port port number on host machine
 */
void
rpcemu_nat_forward_add(PortForwardRule rule)
{
	int i;

	rpclog("Config: Adding NAT forwarding rule %d %u %u\n", rule.type, rule.emu_port, rule.host_port);

	// Detect duplicate rules
	for (i = 0; i < MAX_PORT_FORWARDS; i++) {
		if (port_forward_rules[i].type == rule.type
		    && port_forward_rules[i].emu_port == rule.emu_port)
		{
			rpclog("Config: Discarding duplicate NAT forwarding rule for type %d emu_port %u\n",
			    rule.type, rule.emu_port);
			return;
		}
		if (port_forward_rules[i].type == rule.type
		    && port_forward_rules[i].host_port == rule.host_port)
		{
			rpclog("Config: Discarding duplicate NAT forwarding rule for type %d host_port %u\n",
			    rule.type, rule.host_port);
			return;
		}
	}

	// Find an empty slot and fill it in
	for (i = 0; i < MAX_PORT_FORWARDS; i++) {
		if (port_forward_rules[i].type == PORT_FORWARD_NONE) {
			port_forward_rules[i] = rule;
			return;
		}
	}

	// No slot found for rule
	rpclog("Config: Ran out of space for NAT port forward rules\n");
}

/**
 * Remove a forwarding rule in the NAT
 *
 * @param type      TCP or UDP
 * @param emu_port  port number on emulated machine
 * @param host_port port number on host machine
 */
void
rpcemu_nat_forward_remove(PortForwardRule rule)
{
	int i;

	for (i = 0; i < MAX_PORT_FORWARDS; i++) {
		if (port_forward_rules[i].type == rule.type
		    && port_forward_rules[i].emu_port == rule.emu_port
		    && port_forward_rules[i].host_port == rule.host_port)
		{
			port_forward_rules[i].type      = PORT_FORWARD_NONE;
			port_forward_rules[i].emu_port  = 0;
			port_forward_rules[i].host_port = 0;

			return;
		}
	}

	// rule not found, should be impossible
	assert(0);
}
