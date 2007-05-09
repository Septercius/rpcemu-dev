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

#if defined WIN32 || defined _WIN32 || defined _WIN32
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
	#endif

#else
	#ifdef _GCC
		#define INLINING static inline
	#else
		#define INLINING inline
	#endif
#endif




#define GRAPHICS_TYPE GFX_AUTODETECT_WINDOWED

/*This determines whether RPCemu can use hardware to blit and scale the display.
  If this is disabled then modes lower than 640x480 can look odd, and the system
  is slower. However, this must be commented out on some ports (Linux)*/
#ifndef __unix
#define HARDWAREBLIT
#endif

/*This moves the calls to blit() and stretch_blit() to a seperate thread. It
  gives a large speedup on a dual-core processor when lots of screen data is
  being updated (eg a full 800x600 screen), and improves the sound stability a
  bit. Not sure how it performs on a single core processor.
  This alters vidc20.c a little - when the rest of drawscr() finishes, it sets a
  flag instead of blitting. This is tested by blitterthread(), which must be
  called regularly. If a thread is not created in the platform specific file,
  then no blits happen, and the emulator will hang due to the synchronisation in
  place.
  In Windows, on many systems, this _must_ be enabled. Otherwise mouse & keyboard
  response will be appallingly bad.*/
#ifndef __unix
#define BLITTER_THREAD
#endif

/*This makes the RISC OS mouse pointer follow the host pointer exactly. Useful
  for Linux port, however use mouse capturing if possible - mousehack has some
  bugs*/
#ifdef __unix
#define mousehackena 1
#else
#define mousehackena 1
#endif
#define mousehack (mousehackena&&mousehackon)

/*This enables abort checking after every LDR/STR/LDM/STM instruction in the
  recompiler. Disabling this makes the recompiler check after every block
  instead - this doesn't appear to break RISC OS, but you never know...*/
#define ABORTCHECKING

extern int mousehackon;
//#define PREFETCH



/*Config*/
extern int vrammask;
extern int model;
extern int model2;
extern int rammask;
extern int stretchmode;

extern uint32_t soundaddr[4];

extern uint32_t inscount;
extern int rinscount;
extern int cyccount;

/* rpc-[linux|win].c */
extern void error(const char *format, ...);
extern void rpclog(const char *format, ...);
extern void updatewindowsize(uint32_t x, uint32_t y);
extern void wakeupsoundthread();
extern void wakeupblitterthread();
extern void updateirqs(void);
extern void resetrpc();
extern int quited;

extern char exname[512];
extern int timetolive;
/*rpcemu.c*/
extern int startrpcemu();
extern void execrpcemu();
extern void endrpcemu();

/*Generic*/
extern int lastinscount;
extern int infocus;

/*FPA*/
extern void resetfpa();
extern void dumpfpa(void);
extern void fpaopcode(uint32_t opcode);








#endif
