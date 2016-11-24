/*RPCemu v0.5 by Tom Walker
  Main header file*/

#ifndef _rpc_h
#define _rpc_h

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "iomd.h"
#include "superio.h"

/* URLs used for the help menu weblinks */
#define URL_MANUAL  "http://www.marutan.net/rpcemu/manual/"
#define URL_WEBSITE "http://www.marutan.net/rpcemu/"

#if !defined(_DEBUG) && !defined(NDEBUG)
#define NDEBUG
#endif

/* If we're not using GNU C, elide __attribute__ */
#ifndef __GNUC__
# define __attribute__(x) /*NOTHING*/
#endif

#if defined WIN32 || defined _WIN32
        #define RPCEMU_WIN
	#ifdef _MSC_VER // Microsoft Visual Studio
                #define fseeko64(_a, _b, _c) fseek(_a, (long)_b, _c)
                __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
	#endif
	#define sleep(x) Sleep(x)
#endif


#if defined __MACH__ || defined __OpenBSD__
#define fseeko64(_a, _b, _c) fseeko(_a, _b, _c)
#define ftello64(stream) ftello(stream)
#define fopen64(_a, _b) fopen(_a, _b)
#define off64_t off_t
#endif

#if defined WORDS_BIGENDIAN
	#define _RPCEMU_BIG_ENDIAN
#endif

/* Does this platform support one or more of our networking types? */
/* Note that networking is currently supported on Mac OS X with the Cocoa GUI
   version but not with the Allegro GUI. */
#if defined __linux || defined __linux__ || defined WIN32 || defined _WIN32 || \
    defined RPCEMU_COCOA_GUI
#define RPCEMU_NETWORKING
#endif

/*This makes the RISC OS mouse pointer follow the host pointer exactly. Useful
  for Linux port, however use mouse capturing if possible - mousehack has some
  bugs*/
#define mousehack (config.mousehackon && !fullscreen)

extern int fullscreen;
/*This enables abort checking after every LDR/STR/LDM/STM instruction in the
  recompiler. Disabling this makes the recompiler check after every block
  instead - this doesn't appear to break RISC OS, but you never know...*/
#define ABORTCHECKING

/** The type of networking configured */
typedef enum {
	NetworkType_Off,
	NetworkType_EthernetBridging,
	NetworkType_IPTunnelling,
	NetworkType_MAX
} NetworkType;

/** Selection of models that the emulator can emulate,
  must be kept in sync with models[] array in rpcemu.c
  the size of model_selection gui.c must be Model_MAX */
typedef enum {
	Model_RPCARM610,
	Model_RPCARM710,
	Model_RPCSA110,
	Model_A7000,
	Model_A7000plus,
	Model_RPCARM810,
	Model_Phoebe,
	Model_MAX         /**< Always last entry */
} Model;

/** The type of processor configured */
typedef enum {
	CPUModel_ARM610,
	CPUModel_ARM710,
	CPUModel_SA110,
	CPUModel_ARM7500,
	CPUModel_ARM7500FE,
	CPUModel_ARM810
} CPUModel;

/** The user's configuration of the emulator */
typedef struct {
	unsigned mem_size;	/**< Amount of RAM in megabytes */
	int vrammask;
	char *username;
	char *ipaddress;
	char *macaddress;
	char *bridgename;
	int refresh;		/**< Video refresh rate */
	int soundenabled;
	int cdromenabled;
	int cdromtype;
	char isoname[512];
	int mousehackon;
	int mousetwobutton;	/**< Swap the behaviour of the right and middle
	                             buttons, for mice with two buttons */
	NetworkType network_type;
	int cpu_idle;		/**< Attempt to reduce CPU usage */
} Config;

extern Config config;

/** Structure to hold details about a model that the emulator can emulate */
typedef struct {
	const char	*name_gui;	/**< String used in the GUI */
	const char	*name_config;	/**< String used in the Config file to select model */
	CPUModel	cpu_model;	/**< CPU used in this model */
	IOMDType	iomd_type;	/**< IOMD used in this model */
	SuperIOType	super_type;     /**< SuperIO chip used in this model */
	uint32_t        i2c_devices;    /**< Bitfield of devices on the I2C bus */
} Model_Details;

extern const Model_Details models[]; /**< array of details of models the emulator can emulate */

/** Structure to hold hardware details of the current model being emulated
 (cached values of Model_Details for speed of lookup) */
typedef struct {
	Model		model;		/**< enum value of model */
	CPUModel	cpu_model;	/**< CPU used in this model */
	IOMDType	iomd_type;	/**< IOMD used in this model */
	SuperIOType	super_type;     /**< SuperIO chip used in this model */
	uint32_t        i2c_devices;    /**< Bitfield of devices on the I2C bus */
} Machine;

extern Machine machine; /**< The details of the current model being emulated */

extern uint32_t inscount;
extern int cyccount;

/* These functions can optionally be overridden by a platform. If not
   needed to be overridden, there is a generic version in rpc-machdep.c */
extern const char *rpcemu_get_datadir(void);
extern const char *rpcemu_get_log_path(void);

/* rpc-[linux|win].c */
typedef struct {
	uint64_t	size;		/**< Size of disk */
	uint64_t	free;		/**< Free space on disk */
} disk_info;

extern void fatal(const char *format, ...) __attribute__((noreturn));
extern void error(const char *format, ...);

extern int path_disk_info(const char *path, disk_info *d);

extern void updatewindowsize(uint32_t x, uint32_t y);
extern void updateirqs(void);

extern void sound_thread_wakeup(void);
extern void sound_thread_start(void);
extern void sound_thread_close(void);

/* Additional logging functions (optional) */
extern void rpcemu_log_os(void);

/*rpcemu.c*/
extern int startrpcemu(void);
extern void execrpcemu(void);
extern void rpcemu_idle(void);
extern void endrpcemu(void);
extern void resetrpc(void);
extern void rpcemu_floppy_load(int drive, const char *filename);
extern void rpclog(const char *format, ...);
extern void rpcemu_model_changed(Model model);
extern const char *rpcemu_file_get_extension(const char *filename);

extern int mousecapture;
extern int drawscre;
extern int quited;
extern char discname[2][260];

/* Performance measuring variables */
extern int updatemips;
typedef struct {
	float mips;
	float mhz;
	float tlb_sec;
	float flush_sec;
	uint32_t mips_count;
	float mips_total;
} Perf;
extern Perf perf;

/* UNIMPLEMENTED requires variable argument macros
   GCC extension or C99 */
#if defined(_DEBUG) && (defined(__GNUC__) || __STDC_VERSION__ >= 199901L)
  /**
   * UNIMPLEMENTED
   *
   * Used to report sections of code that have not been implemented yet
   *
   * @param section Section code is missing from eg. "IOMD register" or
   *                "HostFS filecore message"
   * @param format  Section specific information
   * @param ...     Section specific information variable arguments
   */
  #define UNIMPLEMENTED(section, format, args...) \
    UNIMPLEMENTEDFL(__FILE__, __LINE__, (section), (format), ## args)

  void UNIMPLEMENTEDFL(const char *file, unsigned line,
                       const char *section, const char *format, ...);
#else
  /* This function has no corresponding body, the compiler
     is clever enough to use it to swallow the arguments to
     debugging calls */
  void unimplemented_null(const char *section, const char *format, ...);

  #define UNIMPLEMENTED 1?(void)0:(void)unimplemented_null

#endif

/* Acknowledge and prevent -Wunused-parameter warnings on functions
 * where the parameter is part of more generic API */
#define NOT_USED(arg)	(void) arg

/*Generic*/
extern int infocus;

/*FPA*/
extern void resetfpa(void);
extern void fpaopcode(uint32_t opcode);

#endif
