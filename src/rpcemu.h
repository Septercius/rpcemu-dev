/*RPCemu v0.5 by Tom Walker
  Main header file*/

#ifndef _rpc_h
#define _rpc_h

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_DEBUG) && !defined(NDEBUG)
#define NDEBUG
#endif

/* If we're not using GNU C, elide __attribute__ */
#ifndef __GNUC__
# define __attribute__(x) /*NOTHING*/
#endif

#if defined WIN32 || defined _WIN32 || defined _WIN32
        #define RPCEMU_WIN
	#ifdef _MSC_VER // Microsoft Visual Studio
                #ifdef _DEBUG
                    #define INLINING _inline
                #else
                    #define INLINING __forceinline
                #endif
                #define fseeko64(_a, _b, _c) fseek(_a, (long)_b, _c)
                __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
                #define sleep(x) Sleep(x)
	#else
        	#ifdef __GNUC__
        		#define INLINING static inline
        	#else
        		#define INLINING inline
        	#endif
                #define sleep(x) Sleep(x)
	#endif

#else
	#ifdef _GCC
		#define INLINING static inline
	#else
		#define INLINING inline
	#endif
#endif


#ifdef DJGPP
#define fseeko64(_a, _b, _c) fseek(_a, (long)_b, _c)
#endif

#ifdef __MACH__
#define fseeko64(_a, _b, _c) fseeko(_a, _b, _c)
#define ftello64(stream) ftello(stream)
#define fopen64(_a, _b) fopen(_a, _b)
#define off64_t off_t
#endif

#if defined _BIG_ENDIAN || defined __BIG_ENDIAN__
	#define _RPCEMU_BIG_ENDIAN
#endif

#define GRAPHICS_TYPE GFX_AUTODETECT_WINDOWED

/*This determines whether RPCemu can use hardware to blit and scale the display.
  If this is disabled then modes lower than 640x480 can look odd, and the system
  is slower. However, this must be commented out on some ports (Linux)*/
#ifndef __unix
#ifndef DJGPP
#ifndef __MACH__
#define HARDWAREBLIT
#endif
#endif
#endif

/*This moves the calls to blit() and stretch_blit() to a seperate thread. It
  gives a large speedup on a dual-core processor when lots of screen data is
  being updated (eg a full 800x600 screen), and improves the sound stability a
  bit. Not sure how it performs on a single core processor.
  This alters vidc20.c a little - when the rest of drawscr() finishes, it wakes
  up the vidc display thread which then reads from VRAM, converts from VIDC 
  format, then blits to screen.
  In Windows, on many systems, this _must_ be enabled. Otherwise mouse & keyboard
  response will be appallingly bad.*/
#define VIDC_THREAD

/*This makes the RISC OS mouse pointer follow the host pointer exactly. Useful
  for Linux port, however use mouse capturing if possible - mousehack has some
  bugs*/
#define mousehack (config.mousehackon && !fullscreen)

extern int fullscreen;
/*This enables abort checking after every LDR/STR/LDM/STM instruction in the
  recompiler. Disabling this makes the recompiler check after every block
  instead - this doesn't appear to break RISC OS, but you never know...*/
#define ABORTCHECKING

/*This makes RPCemu always run in fullscreen mode. Mostly suitable for DOS*/
#ifdef DJGPP
#define FULLSCREENALWAYS
#endif

/** The type of networking configured */
typedef enum {
	NetworkType_Off,
	NetworkType_EthernetBridging,
	NetworkType_IPTunnelling,
} NetworkType;

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
	CPUModel model;
	int rammask;
	int vrammask;
	int stretchmode;
	char *username;
	char *ipaddress;
	char *macaddress;
	char *bridgename;
	int refresh;		/* Video refresh rate */
	int soundenabled;
	int skipblits;
	int cdromenabled;
	int cdromtype;
	char isoname[512];
	int mousehackon;
	NetworkType network_type;
} Config;

extern Config config;

extern uint32_t inscount;
extern int rinscount;
extern int cyccount;

/* rpc-[linux|win].c */
extern void fatal(const char *format, ...) __attribute__((noreturn));
extern void error(const char *format, ...);
extern void updatewindowsize(uint32_t x, uint32_t y);
extern void updateirqs(void);

extern void sound_thread_wakeup(void);
extern void sound_thread_start(void);
extern void sound_thread_close(void);

extern char exname[512];

/*rpcemu.c*/
extern int startrpcemu(void);
extern void execrpcemu(void);
extern void endrpcemu(void);
extern void resetrpc(void);
extern void rpclog(const char *format, ...);
extern void domips(void);

extern int mousecapture;
extern int drawscre;
extern int quited;

/* Performance measuring variables */
extern int updatemips;
extern float mips, mhz, tlbsec, flushsec;
extern uint32_t mipscount;
extern float mipstotal;

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

/*Generic*/
extern int lastinscount;
extern int infocus;

/*FPA*/
extern void resetfpa(void);
extern void fpaopcode(uint32_t opcode);

#define CDROM_ISO   0
#define CDROM_IOCTL 1

#endif
