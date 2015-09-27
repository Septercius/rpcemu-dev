/* arm_common.c - sections of code that are shared between the interpreted and dynarec builds */

#include <stdint.h>

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

#define SWI_Portable_ReadFeatures	0x42fc5
#define SWI_Portable_Idle		0x42fc6

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
		swinum = arm.reg[10] & 0xdffff;
	} else if (swinum == SWI_OS_CallASWIR12) {
		swinum = arm.reg[12] & 0xdffff;
	}

	/* Intercept RISC OS Portable SWIs to enable RPCEmu to sleep when
	   RISC OS is idle */
	if (config.cpu_idle) {
		switch (swinum) {
		case SWI_Portable_ReadFeatures:
			arm.reg[1] = (1u << 4);	/* Idle supported flag */
			arm.reg[cpsr] &= ~VFLAG;
			return;
		case SWI_Portable_Idle:
			rpcemu_idle();
			arm.reg[cpsr] &= ~VFLAG;
			return;
		}
	}

	if (mousehack && swinum == SWI_OS_Word && arm.reg[0] == 21) {
		if (readmemb(arm.reg[1]) == 1) {
			/* OS_Word 21, 1 Define Mouse Coordinate bounding box */
			mouse_hack_osword_21_1(arm.reg[1]);
			return;
		} else if (readmemb(arm.reg[1]) == 4) {
			/* OS_Word 21, 4 Read unbuffered mouse position */
			mouse_hack_osword_21_4(arm.reg[1]);
			return;
		} else {
			goto realswi;
		}

	} else if (mousehack && swinum == SWI_OS_Mouse) {
		/* OS_Mouse */
		mouse_hack_osmouse();
		arm.reg[cpsr] &= ~VFLAG;

	} else if (swinum == ARCEM_SWI_HOSTFS) {
		ARMul_State state;

		state.Reg = arm.reg;
		hostfs(&state);

	}
#ifdef RPCEMU_NETWORKING
	else if (swinum == ARCEM_SWI_NETWORK) {
		if (config.network_type != NetworkType_Off) {
			network_swi(arm.reg[0], arm.reg[1], arm.reg[2], arm.reg[3],
			            arm.reg[4], arm.reg[5], &arm.reg[0], &arm.reg[1]);
		}
	}
#endif
	else {
realswi:
		if (mousehack && swinum == SWI_OS_Word && arm.reg[0] == 21 &&
		    readmemb(arm.reg[1]) == 0)
		{
			/* OS_Word 21, 0 Define pointer size, shape and active point */
			mouse_hack_osword_21_0(arm.reg[1]);
		}
		if (mousehack && swinum == SWI_OS_Byte && arm.reg[0] == 106) {
			/* OS_Byte 106 Select pointer / activate mouse */
			mouse_hack_osbyte_106(arm.reg[1]);
		}
		exception(SUPERVISOR, 0xc, 4);
	}
}

