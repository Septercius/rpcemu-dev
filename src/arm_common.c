/* arm_common.c - sections of code that are shared between the interpreted and dynarec builds */

#include <stdlib.h>
#include <time.h>

#if defined WIN32 || defined _WIN32
#include <windows.h>
#endif

#include "rpcemu.h"

#include "arm.h"
#include "fdc.h"
#include "ide.h"
#include "iomd.h"
#include "mem.h"
#include "keyboard.h"
#include "hostfs.h"
#include "vidc20.h"

#ifdef RPCEMU_NETWORKING
#include "network.h"
#endif

#define SWI_OS_Byte		0x6
#define SWI_OS_Word		0x7
#define SWI_OS_Mouse		0x1c
#define SWI_OS_CallASWI		0x6f
#define SWI_OS_CallASWIR12	0x71

#define SWI_Portable_ReadFeatures	0x42fc5
#define SWI_Portable_Idle		0x42fc6

/**
 * Attempt to reduce CPU usage by checking for pending interrupts, running
 * any callbacks, and then sleeping for a short period of time.
 *
 * Called when RISC OS calls "Portable_Idle" SWI.
 */
static void
arm_idle(void)
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

	/* Intercept RISC OS Portable SWIs to enable RPCEmu to sleep when
	   RISC OS is idle */
	if (config.cpu_idle) {
		switch (swinum) {
		case SWI_Portable_ReadFeatures:
			armregs[1] = (1u << 4);	/* Idle supported flag */
			armregs[cpsr] &= ~VFLAG;
			return;
		case SWI_Portable_Idle:
			arm_idle();
			armregs[cpsr] &= ~VFLAG;
			return;
		}
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
			network_swi(armregs[0], armregs[1], armregs[2], armregs[3],
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

