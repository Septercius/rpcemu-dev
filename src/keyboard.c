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

/* PS/2 keyboard and mouse emulation */

/*ARM command 0xFF - keyboard enters reset mode
  Sends 0xFA (ack) to ARM
  Performs self-test
  Sends 0xAA after 500-750 ms
  ARM command 0xEE - Echo - keyboard returns 0xEE
  ARM command 0xED - Set/Reset LEDs - following with argument
  ARM command 0xF3 - Set rate/delay - following with argument
  ARM command 0xF4 - Enable keyboard
  ARM command 0xF5 - Disable keyboard - cease scanning
  ARM command 0xF6 - Load default

  Keyboard acks (0xFA) after every byte
  */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "rpcemu.h"
#include "vidc20.h"
#include "mem.h"
#include "iomd.h"
#include "arm.h"
#include "i8042.h"

#ifdef __APPLE__
#include "keyboard_macosx.h"
#endif

/* Keyboard Commands */
#define KBD_CMD_ENABLE		0xf4
#define KBD_CMD_RESET		0xff

/* Keyboard Replies */
#define KBD_REPLY_POR		0xaa	/* Power on reset */
#define KBD_REPLY_ACK		0xfa	/* Command ACK */

/* Mouse Commands */
#define AUX_SET_SCALE11		0xe6	/* Set 1:1 scaling */
#define AUX_SET_SCALE21		0xe7	/* Set 2:1 scaling */
#define AUX_SET_RES		0xe8	/* Set resolution */
#define AUX_GET_TYPE		0xf2	/* Get type */
#define AUX_SET_SAMPLE		0xf3	/* Set sample rate */
#define AUX_ENABLE_DEV		0xf4	/* Enable aux device */
#define AUX_RESEND		0xfe	/* Resend last packet */
#define AUX_RESET		0xff	/* Reset aux device */

/* Mouse Replies */
#define AUX_TEST_OK		0xaa	/* Self-test passed */
#define AUX_ACK			0xfa	/* Command byte acknowledge */

/* Bits within the IOMD PS/2 control/status registers for mouse and keyboard */
#define PS2_CONTROL_DATA_STATE	0x01
#define PS2_CONTROL_CLOCK_STATE	0x02
#define PS2_CONTROL_RXPARITY	0x04
#define PS2_CONTROL_ENABLE	0x08
#define PS2_CONTROL_RX_BUSY	0x10
#define PS2_CONTROL_RX_FULL	0x20
#define PS2_CONTROL_TX_BUSY	0x40
#define PS2_CONTROL_TX_EMPTY	0x80

#define PS2_QUEUE_SIZE 256

int kcallback = 0;
int mcallback = 0;

typedef struct {
	uint8_t	data[PS2_QUEUE_SIZE];
	int	rptr, wptr, count;
} PS2Queue;

static struct {
	int		enable;
	int		reset;
	uint8_t		stat;		/**< PS/2 control register for the keyboard */
	uint8_t		data;		/**< PS/2 data register for the keyboard */
	uint8_t		command;

	PS2Queue	queue;

	int		keys2[128];

#ifdef RPCEMU_MACOSX
	/* Non-zero if the last F12 keydown event was translated into Break because Cmd was pressed */
	int		f12transtobreak;
#endif
} kbd;


static struct {
	int x;		/**< host mouse x (absolute, including doublesize), updated via QT frontend */
	int y;		/**< host mouse y (absolute, including doublesize), updated via QT frontend */
	int buttons;	/**< Mouse button bitfield, updated via QT frontend */
	int dx;		/**< delta of relative mouse movement since previous mouse update message */
	int dy;		/**< delta of relative mouse movement since previous mouse update message */
} mouse;


static int msenable, msreset;
static uint8_t msstat;		/**< PS/2 control register for the mouse */
static uint8_t msdata;		/**< PS/2 data register for the mouse */
static int mousepoll;		/**< Are we in mouse Stream Mode */
static int msincommand;		/**< Used to store the command received that has a data byte following it */
static int justsent;
static PS2Queue msqueue;
static uint8_t mouse_type;	/**< 0 = PS/2, 3 = IMPS/2, 4 = IMEX */
static uint8_t mouse_detect_state;

/* Mousehack variables */
static struct {
	uint8_t	pointer;		/**< Currently selected pointer, 0 = off, 1-4 defined pointer shapes */
	int	activex[5];		/**< click points of selected pointer */
	int	activey[5];		/**< click points of selected pointer */

	int	cursor_linked;		/**< Is the cursor image currently linked to the mouse pointer location */
	int	cursor_unlinked_x;	/**< If cursor and mouse pointer are unlinked the X position of the cursor */
	int	cursor_unlinked_y;	/**< If cursor and mouse pointer are unlinked the Y position of the cursor */

	struct {			/**< Mouse bounding box, defined with OS_Word 21, 1, values are in OS units */
		int16_t	left;
		int16_t	right;
		int16_t	bottom;
		int16_t	top;
	} boundbox;
} mouse_hack;

static inline void
keyboard_irq_rx_raise(void)
{
	if (machine.model == Model_Phoebe) {
		i8042_keyboard_irq_raise();
	} else {
		iomd.irqb.status |= IOMD_IRQB_KEYBOARD_RX;
		updateirqs();
	}
}

static inline void
keyboard_irq_rx_lower(void)
{
	if (machine.model == Model_Phoebe) {
		i8042_keyboard_irq_lower();
	} else {
		iomd.irqb.status &= ~IOMD_IRQB_KEYBOARD_RX;
		updateirqs();
	}
}

static inline void
keyboard_irq_tx_raise(void)
{
	iomd.irqb.status |= IOMD_IRQB_KEYBOARD_TX;
	updateirqs();
}

static inline void
mouse_irq_tx_raise(void)
{
	iomd.irqd.status |= IOMD_IRQD_MOUSE_TX;
	updateirqs();
}

static inline void
mouse_irq_tx_lower(void)
{
	iomd.irqd.status &= ~IOMD_IRQD_MOUSE_TX;
	updateirqs();
}

static inline void
mouse_irq_rx_raise(void)
{
	if (machine.model == Model_Phoebe) {
		i8042_mouse_irq_raise();
	} else {
		iomd.irqd.status |= IOMD_IRQD_MOUSE_RX;
		updateirqs();
	}
}

static inline void
mouse_irq_rx_lower(void)
{
	if (machine.model == Model_Phoebe) {
		i8042_mouse_irq_lower();
	} else {
		iomd.irqd.status &= ~IOMD_IRQD_MOUSE_RX;
		updateirqs();
	}
}

static void
ps2_queue(PS2Queue *q, uint8_t b)
{
	if (q->count >= PS2_QUEUE_SIZE) {
		return;
	}
	q->data[q->wptr] = b;
	if (++q->wptr == PS2_QUEUE_SIZE) {
		q->wptr = 0;
	}
	q->count++;
}

static int
calculateparity(uint8_t v)
{
	int c, d = 0;

	for (c = 0; c < 8; c++) {
		if (v & (1 << c)) {
			d++;
		}
	}
	if (d & 1) {
		return 0;
	}
	return 1;
}

void
keyboard_reset(void)
{
	kcallback = 0;
	memset(&kbd, 0, sizeof(kbd));

	msqueue.rptr = 0;
	msqueue.wptr = 0;
	msqueue.count = 0;
	msenable = 0;
	mcallback = 0;
	msreset = 0;
	msstat = 0;
	msincommand = 0;
	mousepoll = 0;
	justsent = 0;
	mouse_type = 0;
	mouse_detect_state = 0;

	/* Mousehack reset */
	mouse_hack.pointer = 0;
	mouse_hack.cursor_linked = 1;
    
#ifdef __APPLE__
    keyboard_reset_modifiers(0);
#endif
}

static uint8_t
ps2_read_data(PS2Queue *q)
{
	uint8_t val;

	if (q->count == 0) {
		int index = q->rptr - 1;
		if (index < 0) {
			index = PS2_QUEUE_SIZE - 1;
		}
		val = q->data[index];
	} else {
		val = q->data[q->rptr];
		if (++q->rptr == PS2_QUEUE_SIZE) {
			q->rptr = 0;
		}
		q->count--;
	}
	return val;
}

/**
 * Write to the IOMD PS/2 keyboard Data register
 *
 * @param v Value to write
 */
void
keyboard_data_write(uint8_t v)
{
	switch (v) {
	case KBD_CMD_RESET:
		kbd.reset = 2;
		kcallback = 4 * 4;
		break;

	case KBD_CMD_ENABLE:
		kbd.reset = 0;
		kbd.command = KBD_CMD_ENABLE;
		kcallback = 1 * 4;
		break;

	default:
		kbd.command = 1;
		kbd.reset = 0;
		kcallback = 1 * 4;
		break;
	}
}

/**
 * Write to the IOMD PS/2 keyboard Control register
 *
 * @param v Value to write
 */
void
keyboard_control_write(uint8_t v)
{
	if (v && !kbd.enable) {
		kbd.reset = 1;
		kcallback = 5 * 4;
	}
	if (v) {
		kbd.stat |= PS2_CONTROL_ENABLE;
	} else {
		kbd.stat &= ~PS2_CONTROL_ENABLE;
	}
}

static void
keyboardsend(uint8_t v)
{
	kbd.data = v;
	kbd.stat |= PS2_CONTROL_RX_FULL;
	if (calculateparity(v)) {
		kbd.stat |= PS2_CONTROL_RXPARITY;
	} else {
		kbd.stat &= ~PS2_CONTROL_RXPARITY;
	}
	keyboard_irq_rx_raise();
}

/* Cannot be called keyboard_callback() due to allegro name clash */
void
keyboard_callback_rpcemu(void)
{
	PS2Queue *q = &kbd.queue;

	if (kbd.reset == 1) {
		kbd.reset = 0;
		kbd.stat |= PS2_CONTROL_TX_EMPTY;
		keyboard_irq_tx_raise();

	} else if (kbd.reset == 2) {
		kbd.reset = 3;
		// keyboardsend(KBD_REPLY_ACK);
		kcallback = 500 * 4;

	} else if (kbd.reset == 3) {
		kcallback = 0;
		kbd.reset = 0;
		keyboardsend(KBD_REPLY_POR);

	} else switch (kbd.command) {
	case 1:
	case KBD_CMD_ENABLE:
		keyboardsend(KBD_REPLY_ACK);
		kcallback = 0;
		kbd.command = 0;
		break;

	case 0xfe:
		keyboardsend(ps2_read_data(q));
		kcallback = 0;
		if (q->count == 0) {
			kbd.command = 0;
		}
		break;
	}
}

/**
 * Read from the IOMD PS/2 keyboard Status register
 *
 * @return Value of register
 */
uint8_t
keyboard_status_read(void)
{
	return kbd.stat;
}

/**
 * Read from the IOMD PS/2 keyboard Data register
 *
 * @return Value of register
 */
uint8_t
keyboard_data_read(void)
{
	keyboard_irq_rx_lower();
	kbd.stat &= ~PS2_CONTROL_RX_FULL;
	if (kbd.command == 0xfe) {
		kcallback = 5 * 4;
	}
	return kbd.data;
}

/**
 * Write to the IOMD PS/2 mouse Control register
 *
 * @param v Value to write
 */
void
mouse_control_write(uint8_t v)
{
//        printf("Write mouse enable %02X\n",v);

        v &= PS2_CONTROL_ENABLE;
        if (v)// && !msenable)
        {
                msreset=1;
                mcallback=20;
        }
	if (v)
		msstat |= PS2_CONTROL_ENABLE;
	else
		msstat &= ~PS2_CONTROL_ENABLE;
}

/**
 * Write to the IOMD PS/2 mouse Data register
 *
 * @param val Value to write
 */
void
mouse_data_write(uint8_t val)
{
        /* Set BUSY flag, clear EMPTY flag */
        msstat = (msstat & 0x3f) | PS2_CONTROL_TX_BUSY;

	mouse_irq_tx_lower();

        justsent=1;
        if (msincommand)
        {
                switch (msincommand)
                {
		/* Certain commands are followed by a data byte that
		   also needs to be acknowledged */
                case AUX_SET_RES:
			ps2_queue(&msqueue, AUX_ACK);
			msincommand = 0;
			mcallback = 20;
			return;

                case AUX_SET_SAMPLE:
			/* Special values to set sample are used to place
			   the mouse into Intellimouse or Intellimouse Explorer
			   mode */
			switch (mouse_detect_state) {
			default:
			case 0:
				if (val == 200)
					mouse_detect_state = 1;
				break;
			case 1:
				if (val == 100)
					mouse_detect_state = 2;
				else if (val == 200)
					mouse_detect_state = 3;
				else
					mouse_detect_state = 0;
				break;
			case 2:
				if (val == 80)
					mouse_type = 3; /* IMPS/2 */
				mouse_detect_state = 0;
				break;
			case 3:
				if (val == 80)
					mouse_type = 4; /* IMEX */
				mouse_detect_state = 0;
				break;
			}

			ps2_queue(&msqueue, AUX_ACK);
			msincommand = 0;
                        mcallback = 20;
                        return;
                }
        }
        else
        {
                switch (val)
                {
                case AUX_RESET:
                        msreset=2;

			/* Turn off Stream Mode */
                        mousepoll = 0;

                        mcallback=20;
                        break;

                case AUX_RESEND:
                        mcallback=150;
                        break;

                case AUX_ENABLE_DEV:
	                ps2_queue(&msqueue, AUX_ACK);

			/* Turn on Stream Mode */
	                mousepoll = 1;

			mcallback=20;
                        break;

                case AUX_SET_SAMPLE:
                        msincommand = AUX_SET_SAMPLE;
			ps2_queue(&msqueue, AUX_ACK);
                        mcallback=20;
                        break;

                case AUX_GET_TYPE:
			ps2_queue(&msqueue, AUX_ACK);
			ps2_queue(&msqueue, mouse_type);
                        mcallback=20;
                        break;

                case AUX_SET_RES:
                        msincommand = AUX_SET_RES;
			ps2_queue(&msqueue, AUX_ACK);
                        mcallback=20;
                        break;

                case AUX_SET_SCALE21:
			ps2_queue(&msqueue, AUX_ACK);
                        mcallback=20;
                        break;

                case AUX_SET_SCALE11:
			ps2_queue(&msqueue, AUX_ACK);
                        mcallback=20;
                        break;

                default:
                        fatal("Bad mouse command %02X\n", val);
                }
        }
}

/**
 * Read from the IOMD PS/2 mouse Status register
 *
 * @return Value of register
 */
uint8_t
mouse_status_read(void)
{
        return msstat;
}

/**
 * Read from the IOMD PS/2 mouse Data register
 *
 * @return Value of register
 */
uint8_t
mouse_data_read(void)
{
        uint8_t temp = msdata;

        msstat &= ~PS2_CONTROL_RX_FULL;

        mouse_irq_rx_lower();

	/* If there's still more data to send, make sure to call us back the
	   next time */
	if (msqueue.count != 0) {
                mcallback = 20;
        }

        msdata = 0;
        return temp;
}

/**
 * Inform the host that the PS/2 mouse has put some data in the IOMD register
 * for it to read (raises interrupt)
 */
static void
mouse_send(uint8_t v)
{
        msdata = v;

        mouse_irq_rx_raise();

        msstat |= PS2_CONTROL_RX_FULL;

	if (calculateparity(v))
		msstat |= PS2_CONTROL_RXPARITY;
	else
		msstat &= ~PS2_CONTROL_RXPARITY;
}

/**
 * Handle sending queued PS/2 mouse messages to the emulated machine; this is
 * to introduce a slight delay between sent packets.
 *
 * Called once the mcallback variable reaches 0.
 */
void
mouse_ps2_callback(void)
{
	assert(mcallback == 0);

        /* Set EMPTY Flag, clear BUSY flag */
        msstat = (msstat & 0x3f) | PS2_CONTROL_TX_EMPTY;

        if (justsent)
        {
		mouse_irq_tx_raise();

                justsent=0;
        }

        if (msreset==1)
        {
		mouse_irq_tx_raise();

                msreset=3;
                msstat |= PS2_CONTROL_TX_EMPTY;      /* This should be pointless - always set above */
                mcallback=20;
        }
        else if (msreset==2)
        {
                msreset=3;
                mouse_send(AUX_ACK);
                mcallback=40;
        }
        else if (msreset==3)
        {
                mcallback=20;
                mouse_send(AUX_TEST_OK);
                msreset=4;
        }
        else if (msreset==4)
        {
                msreset=0;
                mouse_send(0);
                mcallback=0;
        }
        else
        {
		/* For the callback to be sent, there must be some PS/2 data to send */
		assert(msqueue.count > 0);

		/* Send the next byte of PS/2 data to the host */
                mouse_send(ps2_read_data(&msqueue));
        }
}

/**
 * Main interface to inform the emulated machine of changes in host OS mouse
 * movement (Quadrature & PS/2) and buttons (PS/2 only).
 *
 * This function returns is only called in Capture mouse mode. 'mousehack'
 * uses RISC OS specific SWI interception to provide the mouse data.
 *
 * Called when mouse system recieves mouse movement and button updates
 * from front end
 */
static void
mouse_process(void)
{
	static uint8_t oldmouseb = 0;
	static int oldz = 0;
	int x, y;
	int z, tmpz;
	uint8_t mouseb = mouse.buttons & 7;
	uint8_t b;

	assert(!mousehack);

	/* Get the relative X/Y movements since the last call */
	x = mouse.dx;
	y = mouse.dy;

	/* Get the absolute value of the scroll wheel position */
//	z = mouse_z; /* Allegro */
	z = 0; /* HACK */


	/* Update quadrature mouse */
	iomd.mousex += x;
	iomd.mousey -= y; /* Allegro and RPC Y axis go in opposite directions */

        /* Return if not PS/2 mouse */
        if (machine.model != Model_A7000 && machine.model != Model_A7000plus && machine.model != Model_Phoebe) {
                return;
        }

	/* Are we in PS/2 Stream Mode? */
	if (!mousepoll) {
		return;
	}

	/* Has anything changed from previous poll? */
	if (x == 0 && y == 0 && (mouseb == oldmouseb) &&
	    (mouse_type == 0 || (mouse_type != 0 && z == oldz)))
	{
		return;
	}

        oldmouseb=mouseb;

	/* Maximum range you can fit in one PS/2 movement packet is -256 to 255 */
	/* TODO why not send multiple move packets? */
        if (x<-256) x=-256;
        if (x>255) x=255;
        if (y<-256) y=-256;
        if (y>255) y=255;

        y^=0xFFFFFFFF;
        y++;

	/* Calculate relative scrollwheel position from last call */
	tmpz = oldz - z;
	oldz = z;
	z = tmpz;

	/* Send PS/2 button/movement packet */
	{
		uint8_t tmp;

#ifndef __APPLE__
		if (config.mousetwobutton) {
			/* To help people with only two buttons on their mouse,
			   swap the behaviour of middle and right buttons */
			uint8_t mousel = mouseb & 1;
			uint8_t mouser = (mouseb & 2) >> 1;
			uint8_t mousem = (mouseb & 4) >> 2;

			mouseb = mousel | (mousem << 1) | (mouser << 2);
		}
#endif

		tmp = (mouseb & 7) | 8;

		if (x & 0x100) {
			tmp |= 0x10; /* X overflow bit */
		}
		if (y & 0x100) {
			tmp |= 0x20; /* Y overflow bit */
		}
		ps2_queue(&msqueue, tmp);
	}
	ps2_queue(&msqueue, x & 255);
	ps2_queue(&msqueue, y & 255);

	/* Extra byte for IMPS/2 or IMEX */
	switch (mouse_type) {
	default:
		break;
	case 3:
		if (z > 127)
			z = 127;
		else if (z < -127)
			z = -127;
		ps2_queue(&msqueue, z & 0xff);
		break;
	case 4:
		if (z > 7)
			z = 7;
		else if (z < -7)
			z = -7;
		b = z & 0x0f;
		ps2_queue(&msqueue, b);
		break;
	}

	/* There's data in the queue, make sure we're called back */
	mcallback = 20;
}

/**
 * Absolute coordinates used in mousehack mode

 * @param x
 * @param y
 */
void
mouse_mouse_move(int x, int y)
{
	assert(mousehack);

	mouse.x = x;
	mouse.y = y;
}

/**
 * Relative movements used in mouse capture modes
 * 
 * @param dx
 * @param dy
 */
void
mouse_mouse_move_relative(int dx, int dy)
{
	assert(!mousehack);

	mouse.dx = dx;
	mouse.dy = dy;

	mouse.x += mouse.dx;
	mouse.y += mouse.dy;

	mouse_process();
}

/**
 * 
 * 
 * @param buttons Mouse button bitfield
 */
void
mouse_mouse_press(int buttons)
{
	mouse.buttons |= buttons;

	// Capture mode
	if(!mousehack) {
		mouse_process();
	}
}

/**
 * 
 * 
 * @param buttons Mouse button bitfield
 */
void
mouse_mouse_release(int buttons)
{
	mouse.buttons &= ~buttons;

	// Capture mode
	if(!mousehack) {
		mouse_process();
	}
}

/**
 * Used in iomd for Quadrature mouse button processing
 */
int
mouse_buttons_get(void)
{
	return mouse.buttons;
}

/**
 * @param scan_codes pointer to array of ps2 set2 scan codes
 */
void
keyboard_key_press(const uint8_t *scan_codes)
{
	if (scan_codes == NULL) {
		return;
	}

	ps2_queue(&kbd.queue, scan_codes[0]);
	if (scan_codes[0] == 0xe0) {
		/* 2-byte scan code */
		ps2_queue(&kbd.queue, scan_codes[1]);
	} else if (scan_codes[0] == 0xe1) {
		/* 8-byte scan code (only break key) */
		ps2_queue(&kbd.queue, scan_codes[1]);
		ps2_queue(&kbd.queue, scan_codes[2]);
		ps2_queue(&kbd.queue, scan_codes[3]);
		ps2_queue(&kbd.queue, scan_codes[4]);
		ps2_queue(&kbd.queue, scan_codes[5]);
		ps2_queue(&kbd.queue, scan_codes[6]);
		ps2_queue(&kbd.queue, scan_codes[7]);
	}
	kcallback = 20;
	kbd.command = 0xfe;
}

/**
 * @param scan_codes pointer to array of ps2 set2 scan codes
 */
void
keyboard_key_release(const uint8_t *scan_codes)
{
	if (scan_codes == NULL) {
		return;
	}

	assert(scan_codes[0] != 0xe1); /* Break key has no release code */

	if (scan_codes[1] == 0) {
		/* 1-byte scan code */
		ps2_queue(&kbd.queue, 0xf0); /* key-up modifier */
		ps2_queue(&kbd.queue, scan_codes[0]); /* byte */
	} else {
		/* 2-byte scan code */
		ps2_queue(&kbd.queue, scan_codes[0]); /* first byte */
		ps2_queue(&kbd.queue, 0xf0); /* key-up modifier */
		ps2_queue(&kbd.queue, scan_codes[1]); /* second byte */
	}
	kcallback = 20;
	kbd.command = 0xfe;
}

/* Mousehack functions */

/**
 * Given OS Units for X and Y convert to position within host display
 *
 * @param osx OS unit coordinate
 * @param osy OS unit coordinate
 * @param x filled in with host display coordinate
 * @param y filled in with host display coordinate 
 */
static void
mouse_osunits_to_host(int osx, int osy, int *x, int *y)
{
	int32_t xeig = 1;
	int32_t yeig = 1;
	int double_x;
	int double_y;

	assert(x);
	assert(y);

	/* Are we doubling up any directions in the host display? */
	vidc_get_doublesize(&double_x, &double_y);

	if (double_x) {
		xeig = 2;
	}
	if (double_y) {
		yeig = 2;
	}

	*x = osx >> xeig;
	*y = (((vidc_get_ysize() - 1) << yeig) - osy) >> yeig;
}

/**
 * Get the x and y coords in VIDC and OS units.
 *
 * @param x Mouse x position in vidc units
 * @param y Mouse y position in vidc units
 * @param osx Mouse x position in OS units
 * @param osy Mouse y position in OS units
 */
static void
mouse_get_osxy(int *x, int *y, int *osx, int *osy)
{
	int32_t xeig = 1;
	int32_t yeig = 1;
	int double_x;
	int double_y;
	int host_x; /* host mouse position after placed inside bounding box */
	int host_y;
	int vidc_x; /* host mouse pos in vidc units */
	int vidc_y;
	int lmouse_x = mouse.x;
	int lmouse_y = mouse.y;

	assert(mousehack);

	/* Are we doubling up any directions in the host display? */
	vidc_get_doublesize(&double_x, &double_y);

	if (double_x) {
		xeig = 2;
	}
	if (double_y) {
		yeig = 2;
	}

	*osx = lmouse_x << 1;
	if (*osx > mouse_hack.boundbox.right) {
		*osx = mouse_hack.boundbox.right;
	}
	if (*osx < mouse_hack.boundbox.left) {
		*osx = mouse_hack.boundbox.left;
	}
	*x = *osx >> xeig;

	*osy = ((vidc_get_ysize() -1) << yeig) - (lmouse_y << 1);
	if (*osy < mouse_hack.boundbox.bottom) {
		*osy = mouse_hack.boundbox.bottom;
	}
	if (*osy > mouse_hack.boundbox.top) {
		*osy = mouse_hack.boundbox.top;
	}
	*y = (((vidc_get_ysize() -1) << yeig) - *osy) >> yeig;

	/* Where should we place the host pointer, based on the position inside the RO bounding box? */
	host_x = *x;
	host_y = *y;

	/* Calculate the host mouse coordinates in vidc units */
	vidc_x = lmouse_x;
	vidc_y = lmouse_y;

	if (double_x) {
		vidc_x >>= 1;
	}
	if (double_y) {
		vidc_y >>= 1;
	}

	/* If the host mouse isn't where where the risc os bounding box has placed it,
	   place the host pointer inside the bounding box */ 
	if ((vidc_x != *x) || (vidc_y != *y))
	{
		int screen_osx = vidc_get_xsize() << xeig; /* Screen size in OS units */
		int screen_osy = vidc_get_ysize() << yeig;

		/* Restrict the host pointer to the bounding box, unless the 
		   box is greater than or equal to the full screen size */
		if ((mouse_hack.boundbox.left >= 0)
		    || (mouse_hack.boundbox.right < screen_osx)
		    || (mouse_hack.boundbox.bottom >= 0)
		    || (mouse_hack.boundbox.top < screen_osy))
		{
			rpcemu_move_host_mouse(host_x, host_y);
		}
	}
}

/**
 * OS_Word 21, 4 Read unbuffered mouse position
 *
 * Called from arm_common.c SWI handler
 *
 * @param a Address of OS_Word 21 parameter block (to be filled in)
 */
void
mouse_hack_osword_21_4(uint32_t a)
{
        int x;
        int y;
        int osx;
        int osy;

        assert(mousehack);

        mouse_get_osxy(&x, &y, &osx, &osy);

	mem_write8(a + 1, (uint8_t) osy);
	mem_write8(a + 2, (uint8_t) (osy >> 8));
	mem_write8(a + 3, (uint8_t) osx);
	mem_write8(a + 4, (uint8_t) (osx >> 8));
}

/**
 * Return the X/Y active point of the cursor.
 *
 * Used by VIDC to determine if the cursor has moved since its last redraw.
 *
 * @param x Filled in with X coordinate of cursor in native units
 * @param y Filled in with Y coordinate of cursor in native units
 */
void
mouse_hack_get_pos(int *x, int *y)
{
        int osx;
        int osy;

        assert(mousehack);

	if (mouse_hack.cursor_linked) {
		/* Cursor is at current mouse pointer pos */
		mouse_get_osxy(x, y, &osx, &osy);

		*x -= mouse_hack.activex[mouse_hack.pointer];
		*y -= mouse_hack.activey[mouse_hack.pointer];
	} else {
		/* Cursor has been detached from mouse pointer and is independent and not moving */
		*x = mouse_hack.cursor_unlinked_x;
		*y = mouse_hack.cursor_unlinked_y;
	}
}

/**
 * OS_Word 21, 0 Define pointer size, shape and active point
 *
 * Called from arm_common.c SWI handler
 *
 * @param a Address of OS_Word 21 parameter block
 */
void
mouse_hack_osword_21_0(uint32_t a)
{
	uint8_t pointer = mem_read8(a + 1);

        assert(mousehack);

	/* Reject any pointer shapes not in range 0-4 */
	if (pointer > 4) {
		return;
	}

	mouse_hack.activex[pointer] = mem_read8(a + 4);
	mouse_hack.activey[pointer] = mem_read8(a + 5);
}

/**
 * OS_Byte 106 Select pointer / activate mouse
 *
 * Called from arm_common.c SWI handler
 *
 * @param a Pointer shape and linkage flag
 */
void
mouse_hack_osbyte_106(uint32_t a)
{
	assert(mousehack);

	/* Bits 0-6 Select pointer number (1 to 4, or 0 to turn off)
	   Bit  7   Unlink visible pointer from mouse, if set */

	/* If Bits 0-6 are outside the range 0 to 4 then ignore, because the
	   pointer number is invalid and RISC OS ignores it. */
	if ((a & 0x7f) > 4)
		return;

	mouse_hack.pointer = a & 0x7f; /* Obtain pointer number (range 0 to 4) */
	/* pointer should now contain selected number 1-4 or 0 if turned off */
	assert(mouse_hack.pointer <= 4);

	/* Bit 7 =  Unlink visible pointer from mouse */
	if (a & 0x80) {
		/* Remember the location of the cursor as the mouse pointer is now independent */
		mouse_hack_get_pos(&mouse_hack.cursor_unlinked_x, &mouse_hack.cursor_unlinked_y);

		mouse_hack.cursor_linked = 0;
	} else {
		mouse_hack.cursor_linked = 1;
	}

}

/**
 * mousehack handler of OS_Mouse SWI
 *
 * Fill in SWI return values with x, y and button info
 *
 * Called from arm_common.c SWI handler on OS_Mouse
 */
void
mouse_hack_osmouse(void)
{
	int32_t temp_x;
	int32_t temp_y;
	uint32_t buttons = 0;
	int32_t yeig = 1;
	int double_x;
	int double_y;

	assert(mousehack);

	/* Are we doubling up any directions in the host display? */
	vidc_get_doublesize(&double_x, &double_y);

	/* Mouse X coordinate */
	temp_x = mouse.x << 1;
	if (temp_x > mouse_hack.boundbox.right) {
		temp_x = mouse_hack.boundbox.right;
	}
	if (temp_x < mouse_hack.boundbox.left) {
		temp_x = mouse_hack.boundbox.left;
	}
	arm.reg[0] = (uint32_t) temp_x;

	/* Mouse Y coordinate */
	if (double_y) {
		yeig = 2;
	}
	temp_y = (vidc_get_ysize() << yeig) - (mouse.y << 1);
	if (temp_y < mouse_hack.boundbox.bottom) {
		temp_y = mouse_hack.boundbox.bottom;
	}
	if (temp_y > mouse_hack.boundbox.top) {
		temp_y = mouse_hack.boundbox.top;
	}
	arm.reg[1] = (uint32_t) temp_y;

	/* Mouse buttons */
	if (mouse.buttons & 1) { 
		buttons |= 4;			/* Left button */
	}
    
#ifdef __APPLE__
    if (mouse.buttons & 2)
    {
        buttons |= 1;        /* Right button */
    }
    if (mouse.buttons & 4)
    {
        buttons |= 2;        /* Middle button */
    }
#else
	if (config.mousetwobutton) {
		/* To help people with only two buttons on their mouse, swap
		   the behaviour of middle and right buttons */
		if (mouse.buttons & 2) {
			buttons |= 2;		/* Middle button */
		}
		if (mouse.buttons & 4) {
			buttons |= 1;		/* Right button */
		}
	} else {
		if (mouse.buttons & 2) {
			buttons |= 1;		/* Right button */
		}
		if (mouse.buttons & 4) {
			buttons |= 2; 		/* Middle button */
		}
	}
#endif
    
	arm.reg[2] = buttons;

	arm.reg[3] = 0; /* R3 = time of button change */
}

/**
 * OS_Word 21, 1 Define Mouse Coordinate bounding box
 *
 * Called from arm_common.c SWI handler
 *
 * @param a Address of OS_Word 21 parameter block
 */
void
mouse_hack_osword_21_1(uint32_t a)
{
	/* This is called regardless of whether or not we're in mousehack
	   as it allows 'fullscreen' or 'mouse capture mode' risc os mode changes
	   to have their boxes cached, allowing mousehack to work when you change
	   back to it */

	mouse_hack.boundbox.left   = mem_read8(a + 1) | (mem_read8(a + 2) << 8);
	mouse_hack.boundbox.bottom = mem_read8(a + 3) | (mem_read8(a + 4) << 8);
	mouse_hack.boundbox.right  = mem_read8(a + 5) | (mem_read8(a + 6) << 8);
	mouse_hack.boundbox.top    = mem_read8(a + 7) | (mem_read8(a + 8) << 8);
}

/**
 * OS_Word 21, 3 Move mouse position
 *
 * Called from arm_common.c SWI handler
 *
 * @param a Address of OS_Word 21 parameter block
 */
void
mouse_hack_osword_21_3(uint32_t a)
{
	int16_t osx;
	int16_t osy;
	int x;
	int y;

	assert(mousehack);

	osx = mem_read8(a + 1) | (mem_read8(a + 2) << 8);
	osy = mem_read8(a + 3) | (mem_read8(a + 4) << 8);

	mouse_osunits_to_host(osx, osy, &x, &y);

	rpcemu_move_host_mouse(x, y);
}
