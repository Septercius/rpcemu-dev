/* arm_common.c - sections of code that are shared between the interpreted and dynarec builds */

#include <stdint.h>

#include "rpcemu.h"

#include "arm.h"
#include "arm_common.h"
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
 * Perform a Store Multiple register operation when the S flag is clear.
 *
 * @param opcode    Opcode of instruction being emulated
 * @param address   The address to be used for the first transfer
 * @param writeback The value to be written to the base register if Writeback
 *                  is requested
 */
void
arm_store_multiple(uint32_t opcode, uint32_t address, uint32_t writeback)
{
	uint32_t orig_base, addr, mask;
	int c;

	orig_base = arm.reg[RN];

	addr = address & ~3;

	/* Store first register */
	mask = 1;
	for (c = 0; c < 15; c++) {
		if (opcode & mask) {
			mem_write32(addr, arm.reg[c]);
			addr += 4;
			break;
		}
		mask <<= 1;
	}
	mask <<= 1;
	c++;

	/* Perform Writeback (if requested) at end of 2nd cycle */
	if (!arm.stm_writeback_at_end && (opcode & (1 << 21)) && (RN != 15)) {
		arm.reg[RN] = writeback;
	}

	/* Check for Abort from first Store */
	if (armirq & 0x40) {
		goto data_abort;
	}

	/* Store remaining registers up to R14 */
	for ( ; c < 15; c++) {
		if (opcode & mask) {
			mem_write32(addr, arm.reg[c]);
			if (armirq & 0x40) {
				goto data_abort;
			}
			addr += 4;
		}
		mask <<= 1;
	}

	/* Store R15 (if requested) */
	if (opcode & (1 << 15)) {
		mem_write32(addr, arm.reg[15] + arm.r15_diff);
		if (armirq & 0x40) {
			goto data_abort;
		}
	}

	/* Perform Writeback (if requested) at end of instruction (SA110) */
	if (arm.stm_writeback_at_end && (opcode & (1 << 21)) && (RN != 15)) {
		arm.reg[RN] = writeback;
	}

	/* No Data Abort */
	return;

	/* A Data Abort occurred, restore the Base Register to the value it
	   had before the instruction */
data_abort:
	if (arm.abort_base_restored && (opcode & (1u << 21)) && (RN != 15)) {
		arm.reg[RN] = orig_base;
	}
}

/**
 * Perform a Store Multiple register operation when the S flag is set.
 *
 * The registers to be stored will be taken from the User bank instead of the
 * current bank.
 *
 * @param opcode    Opcode of instruction being emulated
 * @param address   The address to be used for the first transfer
 * @param writeback The value to be written to the base register if Writeback
 *                  is requested
 */
void
arm_store_multiple_s(uint32_t opcode, uint32_t address, uint32_t writeback)
{
	uint32_t orig_base, addr, mask;
	int c;

	orig_base = arm.reg[RN];

	addr = address & ~3;

	/* Store first register */
	mask = 1;
	for (c = 0; c < 15; c++) {
		if (opcode & mask) {
			mem_write32(addr, *usrregs[c]);
			addr += 4;
			break;
		}
		mask <<= 1;
	}
	mask <<= 1;
	c++;

	/* Perform Writeback (if requested) at end of 2nd cycle */
	if (!arm.stm_writeback_at_end && (opcode & (1 << 21)) && (RN != 15)) {
		arm.reg[RN] = writeback;
	}

	/* Check for Abort from first Store */
	if (armirq & 0x40) {
		goto data_abort;
	}

	/* Store remaining registers up to R14 */
	for ( ; c < 15; c++) {
		if (opcode & mask) {
			mem_write32(addr, *usrregs[c]);
			if (armirq & 0x40) {
				goto data_abort;
			}
			addr += 4;
		}
		mask <<= 1;
	}

	/* Store R15 (if requested) */
	if (opcode & (1 << 15)) {
		mem_write32(addr, arm.reg[15] + arm.r15_diff);
		if (armirq & 0x40) {
			goto data_abort;
		}
	}

	/* Perform Writeback (if requested) at end of instruction (SA110) */
	if (arm.stm_writeback_at_end && (opcode & (1 << 21)) && (RN != 15)) {
		arm.reg[RN] = writeback;
	}

	/* No Data Abort */
	return;

	/* A Data Abort occurred, restore the Base Register to the value it
	   had before the instruction */
data_abort:
	if (arm.abort_base_restored && (opcode & (1u << 21)) && (RN != 15)) {
		arm.reg[RN] = orig_base;
	}
}

/**
 * Perform a Load Multiple register operation when the S flag is clear.
 *
 * @param opcode    Opcode of instruction being emulated
 * @param address   The address to be used for the first transfer
 * @param writeback The value to be written to the base register if Writeback
 *                  is requested
 */
void
arm_load_multiple(uint32_t opcode, uint32_t address, uint32_t writeback)
{
	uint32_t orig_base, addr, mask, temp;
	int c;

	orig_base = arm.reg[RN];

	addr = address & ~3;

	/* Perform Writeback (if requested) */
	if ((opcode & (1 << 21)) && (RN != 15)) {
		arm.reg[RN] = writeback;
	}

	/* Load registers up to R14 */
	mask = 1;
	for (c = 0; c < 15; c++) {
		if (opcode & mask) {
			temp = mem_read32(addr);
			if (armirq & 0x40) {
				goto data_abort;
			}
			arm.reg[c] = temp;
			addr += 4;
		}
		mask <<= 1;
	}

	/* Load R15 (if requested) */
	if (opcode & (1 << 15)) {
		temp = mem_read32(addr);
		if (armirq & 0x40) {
			goto data_abort;
		}
		/* Only update R15 if no Data Abort occurred */
		arm.reg[15] = (arm.reg[15] & ~r15mask) |
		              ((temp + 4) & r15mask);
	}

	/* No Data Abort */
	return;

	/* A Data Abort occurred, modify the Base Register */
data_abort:
	if (!arm.abort_base_restored && (opcode & (1u << 21)) && (RN != 15)) {
		arm.reg[RN] = writeback;
	} else {
		arm.reg[RN] = orig_base;
	}
}

/**
 * Perform a Load Multiple register operation when the S flag is set.
 *
 * If R15 is in the list of registers to be loaded, the PSR flags will be
 * updated as well, subject to the current privilege level.
 *
 * If R15 is not in the list of registers to be loaded, the values will be
 * loaded into the User bank instead of the current bank.
 *
 * @param opcode    Opcode of instruction being emulated
 * @param address   The address to be used for the first transfer
 * @param writeback The value to be written to the base register if Writeback
 *                  is requested
 */
void
arm_load_multiple_s(uint32_t opcode, uint32_t address, uint32_t writeback)
{
	uint32_t orig_base, addr, mask, temp;
	int c;

	orig_base = arm.reg[RN];

	addr = address & ~3;

	/* Perform Writeback (if requested) */
	if ((opcode & (1 << 21)) && (RN != 15)) {
		arm.reg[RN] = writeback;
	}

	mask = 1;
	/* Is R15 in the list of registers to be loaded? */
	if (opcode & (1 << 15)) {
		/* R15 in list - Load registers up to R14 */
		for (c = 0; c < 15; c++) {
			if (opcode & mask) {
				temp = mem_read32(addr);
				if (armirq & 0x40) {
					goto data_abort;
				}
				arm.reg[c] = temp;
				addr += 4;
			}
			mask <<= 1;
		}

		/* Perform load of R15 and update CPSR/flags */
		temp = mem_read32(addr);
		if (armirq & 0x40) {
			goto data_abort;
		}
		arm_write_r15(opcode, temp);

	} else {
		/* R15 not in list - Perform load into User Bank */
		for (c = 0; c < 15; c++) {
			if (opcode & mask) {
				temp = mem_read32(addr);
				if (armirq & 0x40) {
					goto data_abort;
				}
				*usrregs[c] = temp;
				addr += 4;
			}
			mask <<= 1;
		}
	}

	/* No Data Abort */
	return;

	/* A Data Abort occurred, modify the Base Register */
data_abort:
	if (!arm.abort_base_restored && (opcode & (1u << 21)) && (RN != 15)) {
		arm.reg[RN] = writeback;
	} else {
		arm.reg[RN] = orig_base;
	}
}

#ifndef TEST
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
		if (mem_read8(arm.reg[1]) == 1) {
			/* OS_Word 21, 1 Define Mouse Coordinate bounding box */
			mouse_hack_osword_21_1(arm.reg[1]);
			return;
		} else if (mem_read8(arm.reg[1]) == 4) {
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
		    mem_read8(arm.reg[1]) == 0)
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
#endif /* ifndef TEST */
