/* arm_common.c - sections of code that are shared between the interpreted and dynarec builds */

#include <stdlib.h>
#include <time.h>

#if defined WIN32 || defined _WIN32
#include <windows.h>
#endif

#include "rpcemu.h"

#include "arm.h"
#include "mem.h"
#include "keyboard.h"
#include "hostfs.h"

#ifdef RPCEMU_NETWORKING
#include "network.h"
#endif

#define SWI_OS_Byte		0x6
#define SWI_OS_Word		0x7
#define SWI_OS_Mouse		0x1c
#define SWI_OS_CallASWI		0x6f
#define SWI_OS_CallASWIR12	0x71

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
	uint32_t swinum = opcode & 0xdffff;

	inscount++;

	/* Get actual SWI number from OS_CallASWI and OS_CallASWIR12 */
	if (swinum == SWI_OS_CallASWI) {
		swinum = armregs[10] & 0xdffff;
	} else if (swinum == SWI_OS_CallASWIR12) {
		swinum = armregs[12] & 0xdffff;
	}

	if (mousehack && swinum == SWI_OS_Word && armregs[0] == 21) {
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

	} else if (mousehack && swinum == SWI_OS_Mouse) {
		/* OS_Mouse */
		mouse_hack_osmouse();
		armregs[cpsr] &= ~VFLAG;

	} else if (swinum == ARCEM_SWI_HOSTFS) {
		ARMul_State state;

		state.Reg = armregs;
		hostfs(&state);

	}
#ifdef RPCEMU_NETWORKING
	else if (swinum == ARCEM_SWI_NETWORK) {
		if (config.network_type != NetworkType_Off) {
			networkswi(armregs[0], armregs[1], armregs[2], armregs[3],
			           armregs[4], armregs[5], &armregs[0], &armregs[1]);
		}
	}
#endif
	else {
realswi:
		if (mousehack && swinum == SWI_OS_Word && armregs[0] == 21 &&
		    readmemb(armregs[1]) == 0)
		{
			/* OS_Word 21, 0 Define pointer size, shape and active point */
			mouse_hack_osword_21_0(armregs[1]);
		}
		if (mousehack && swinum == SWI_OS_Byte && armregs[0] == 106) {
			/* OS_Byte 106 Select pointer / activate mouse */
			mouse_hack_osbyte_106(armregs[1]);
		}
		exception(SUPERVISOR, 0xc, 4);
	}
}

