/* arm_common.c - sections of code that are shared between the interpreted and dynarec builds */

#include <stdlib.h>
#include <time.h>

#if defined WIN32 || defined _WIN32
#include <windows.h>
#endif

#include "rpcemu.h"

#include "config.h"

#include "arm.h"
#include "mem.h"
#include "keyboard.h"
#include "hostfs.h"

#ifdef RPCEMU_NETWORKING
#include "network.h"
#endif

/**
 * Handler for SWI instructions; includes all the emulator specific SWIs as
 * well as the standard SWI interface of raising an exception.
 *
 * Called from dynarec and interpreted code modes.
 *
 * @param opcode Opcode of instruction being emulated
 */
void
opSWI(uint32_t opcode)
{
	uint32_t templ;

	inscount++;
	rinscount++;
	templ = opcode & 0xdffff;

	if (mousehack && templ == 7 && armregs[0] == 0x15) {
		if (readmemb(armregs[1]) == 1) {
			/* OS_Word 21, 1 Define Mouse Coordinate bounding box */
			mouse_hack_osword_21_1(armregs[1]);
			return;
		} else if (readmemb(armregs[1]) == 4) {
			/* OS_Word 21, 4 Read unbuffered mouse position */
			mouse_hack_osword_21_4(armregs[1]);
			return;
		} else {
			goto realswi;
		}

	} else if (mousehack && templ == 0x1c) {
		/* OS_Mouse */
		mouse_hack_osmouse();
		armregs[cpsr] &= ~VFLAG;

	} else if (templ == ARCEM_SWI_HOSTFS) {
		ARMul_State state;

		state.Reg = armregs;
		hostfs(&state);

	} else if (templ == ARCEM_SWI_NANOSLEEP) {
#ifdef RPCEMU_WIN
		Sleep(armregs[0] / 1000000);
#else
		struct timespec tm;

		tm.tv_sec = 0;
		tm.tv_nsec = armregs[0];
		nanosleep(&tm, NULL);
#endif
		armregs[cpsr] &= ~VFLAG;
	}
#ifdef RPCEMU_NETWORKING
	else if (templ == ARCEM_SWI_NETWORK) {
		if (config.network_type != NetworkType_Off) {
			networkswi(armregs[0], armregs[1], armregs[2], armregs[3],
			           armregs[4], armregs[5], &armregs[0], &armregs[1]);
		}
	}
#endif
	else {
realswi:
		if (mousehack && templ == 7 && armregs[0] == 0x15 &&
		    readmemb(armregs[1]) == 0)
		{
			/* OS_Word 21, 0 Define pointer size, shape and active point */
			mouse_hack_osword_21_0(armregs[1]);
		}
		if (mousehack && templ == 6 && armregs[0] == 106) {
			/* OS_Byte 106 Select pointer / activate mouse */
			mouse_hack_osbyte_106(armregs[1]);
		}
		exception(SUPERVISOR, 0xc, 4);
	}
}

