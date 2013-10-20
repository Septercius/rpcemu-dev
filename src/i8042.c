/*

 Emulation of the Intel 8042 keyboard controller embedded in the
 SMSC FDC37C672 SuperIO chip on the Phoebe.

 References
  - SMSC FDC37C672 datasheet
  - OSDev Wiki - http://wiki.osdev.org/%228042%22_PS/2_Controller

*/
#include <assert.h>
#include <stdint.h>
#include <allegro.h>
#include <stdio.h>
#include "rpcemu.h"
#include "iomd.h"
#include "superio.h"

#include "keyboard.h"
#include "i8042.h"

/* Commands */
#define KBD_CCMD_READ_MODE	0x20	/* Read mode bits */
#define KBD_CCMD_WRITE_MODE	0x60	/* Write mode bits */
#define KBD_CCMD_GET_VERSION	0xa1	/* Get controller version */
#define KBD_CCMD_MOUSE_DISABLE	0xa7	/* Disable mouse interface */
#define KBD_CCMD_MOUSE_ENABLE	0xa8	/* Enable mouse interface */
#define KBD_CCMD_TEST_MOUSE	0xa9	/* Mouse interface test */
#define KBD_CCMD_SELF_TEST	0xaa	/* Controller self test */
#define KBD_CCMD_KBD_TEST	0xab	/* Keyboard interface test */
#define KBD_CCMD_KBD_DISABLE	0xad	/* Keyboard interface disable */
#define KBD_CCMD_KBD_ENABLE	0xae	/* Keyboard interface enable */
#define KBD_CCMD_READ_INPORT	0xc0	/* Read input port */
#define KBD_CCMD_READ_OUTPORT	0xd0	/* Read output port */
#define KBD_CCMD_WRITE_OUTPORT	0xd1	/* Write output port */
#define KBD_CCMD_WRITE_OBUF	0xd2
#define KBD_CCMD_WRITE_AUX_OBUF	0xd3	/* Write to output buffer as if
					   initiated by the auxiliary device */
#define KBD_CCMD_WRITE_MOUSE	0xd4	/* Write the following byte to the mouse */
#define KBD_CCMD_RESET		0xfe	/* Pulse bit 0 of the output port P2 = CPU reset. */
#define KBD_CCMD_NO_OP		0xff	/* Pulse no bits of the output port P2. */

/* Status Register bits */
#define STAT_OFULL	0x01	/* Keyboard output buffer full */
#define STAT_IFULL	0x02	/* Keyboard input buffer full */
#define STAT_SYSFLAG	0x04
#define STAT_CMD	0x08
#define STAT_LOCK	0x10
#define STAT_MFULL	0x20
#define STAT_TTIMEOUT	0x20
#define STAT_RTIMEOUT	0x40
#define STAT_PARITY	0x80

/* Mode bits */
#define KBD_MODE_KBD_INT	0x01	/* Keyboard data generate IRQ */
#define KBD_MODE_MOUSE_INT	0x02	/* Mouse data generate IRQ */
#define KBD_MODE_SYS		0x04	/* The system flag (?) */
#define KBD_MODE_DISABLE_KBD	0x10	/* Disable keyboard interface */
#define KBD_MODE_DISABLE_MOUSE	0x20	/* Disable mouse interface */

/* Interrupt values */
#define SMI_IRQ2_MINT	0x01
#define SMI_IRQ2_KINT	0x02

static struct {
	uint8_t	command;
	uint8_t	status;
	uint8_t	out;
	uint8_t	mode;
	int	irq_kbd;
	int	irq_mouse;
} i8042;

static void
i8042_irq_update(void)
{
	if (i8042.irq_kbd && !(i8042.mode & KBD_MODE_DISABLE_KBD)) {
		/* Generate keyboard interrupt */
		superio_smi_setint2(SMI_IRQ2_KINT);
		superio_smi_clrint2(SMI_IRQ2_MINT);

	} else if (i8042.irq_mouse && !(i8042.mode & KBD_MODE_DISABLE_MOUSE)) {
		/* Generate mouse interrupt */
		superio_smi_setint2(SMI_IRQ2_MINT);
		superio_smi_clrint2(SMI_IRQ2_KINT);

	} else {
		/* Clear keyboard and mouse interrupts */
		superio_smi_clrint2(SMI_IRQ2_KINT);
		superio_smi_clrint2(SMI_IRQ2_MINT);
	}
}

void
i8042_keyboard_irq_raise(void)
{
	i8042.irq_kbd = 1;
	i8042_irq_update();
}

void
i8042_keyboard_irq_lower(void)
{
	i8042.irq_kbd = 0;
	i8042_irq_update();
}

void
i8042_mouse_irq_raise(void)
{
	i8042.irq_mouse = 1;
	i8042_irq_update();
}

void
i8042_mouse_irq_lower(void)
{
	i8042.irq_mouse = 0;
	i8042_irq_update();
}

uint8_t
i8042_data_read(void)
{
	uint8_t result;

	if (i8042.irq_kbd) {
		result = keyboard_data_read();
		i8042.irq_kbd = 0;
	} else if (i8042.irq_mouse) {
		result = mouse_data_read();
		i8042.irq_mouse = 0;
	} else {
		result = i8042.out;
		i8042.status &= ~STAT_OFULL;
	}

	return result;
}

void
i8042_data_write(uint8_t val)
{
	switch (i8042.command) {
	case 0:
		keyboard_data_write(val);
		break;
	case KBD_CCMD_WRITE_MODE:
		i8042.mode = val;
		break;
	case KBD_CCMD_WRITE_MOUSE:
		mouse_data_write(val);
		break;
	default:
		exit(1);
	}
	i8042.command = 0;
}

uint8_t
i8042_status_read(void)
{
	return i8042.status;
}

void
i8042_command_write(uint8_t val)
{
	switch (val) {
	case KBD_CCMD_READ_MODE:
		i8042.out = i8042.mode;
		i8042.status |= STAT_OFULL;
		break;
	case KBD_CCMD_WRITE_MODE:
		i8042.command = val;
		break;
	case KBD_CCMD_MOUSE_DISABLE:
		i8042.mode |= KBD_MODE_DISABLE_MOUSE;
		break;
	case KBD_CCMD_MOUSE_ENABLE:
		i8042.mode &= ~KBD_MODE_DISABLE_MOUSE;
		break;
	case KBD_CCMD_SELF_TEST:
		i8042.out = 0x55;
		i8042.status |= STAT_OFULL;
		break;
	case KBD_CCMD_KBD_DISABLE:
		i8042.mode |= KBD_MODE_DISABLE_KBD;
		break;
	case KBD_CCMD_KBD_ENABLE:
		i8042.mode &= ~KBD_MODE_DISABLE_KBD;
		break;
	case KBD_CCMD_WRITE_MOUSE:
		i8042.command = val;
		break;
	default:
		exit(1);
	}
}

/**
 * Called on program startup and emulated machine reset to initialise the
 * state of the 8042 controller.
 */
void
i8042_reset(void)
{
	i8042.status = STAT_CMD;
	i8042.command = 0;
	i8042.mode = KBD_MODE_KBD_INT | KBD_MODE_MOUSE_INT;
	i8042.out = 0;
	i8042.irq_kbd = 0;
	i8042.irq_mouse = 0;
}
