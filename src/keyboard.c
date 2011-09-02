/*RPCemu v0.6 by Tom Walker
  PS/2 keyboard and mouse emulation*/

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
#include <allegro.h>
#include "rpcemu.h"
#include "vidc20.h"
#include "mem.h"
#include "iomd.h"
#include "arm.h"

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

typedef struct {
        uint8_t	data[PS2_QUEUE_SIZE];
        int	rptr, wptr, count;
} PS2Queue;

static int kbdenable, kbdreset;
static uint8_t kbdstat;		/**< PS/2 control register for the keyboard */
static uint8_t kbddata;		/**< PS/2 data register for the keyboard */
static unsigned char kbdcommand;
static int keys2[128];
static PS2Queue kbdqueue;

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
static int point;

static int cursor_linked;       /**< Is the cursor image currently linked to the mouse pointer location */
static int cursor_unlinked_x;   /**< If cursor and mouse pointer are unlinked the X position of the cursor */
static int cursor_unlinked_y;   /**< If cursor and mouse pointer are unlinked the Y position of the cursor */

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
	iomd.irqd.status |= IOMD_IRQD_MOUSE_RX;
	updateirqs();
}

static inline void
mouse_irq_rx_lower(void)
{
	iomd.irqd.status &= ~IOMD_IRQD_MOUSE_RX;
	updateirqs();
}

static void
ps2_queue(PS2Queue *q, unsigned char b)
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

static int calculateparity(unsigned char v)
{
        int c,d=0;
        for (c=0;c<8;c++)
        {
                if (v&(1<<c)) d++;
        }
        if (d&1) return 0;
        return 1;
}

void resetkeyboard(void)
{
        int c;
        kbdenable=0;
        kcallback=0;
        kbdreset=0;
        kbdstat=0;
        kbdcommand = 0;
        kbdqueue.rptr = 0;
        kbdqueue.wptr = 0;
        kbdqueue.count = 0;
        msqueue.rptr = 0;
        msqueue.wptr = 0;
        msqueue.count = 0;
        msenable=0;
        mcallback=0;
        msreset=0;
        msstat=0;
	msincommand = 0;
	mousepoll = 0;
	justsent = 0;
	mouse_type = 0;
	mouse_detect_state = 0;
        for (c=0;c<128;c++)
            keys2[c]=0;

	/* Mousehack reset */
	point = 0;
	cursor_linked = 1;
}

static uint8_t
ps2_read_data(PS2Queue *q)
{
        uint8_t val;

        if (q->count == 0) {
                int index = q->rptr - 1;
                if (index < 0)
                        index = PS2_QUEUE_SIZE - 1;
                val = q->data[index];
        } else {
                val = q->data[q->rptr];
                if (++q->rptr == PS2_QUEUE_SIZE)
                        q->rptr = 0;
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
//        rpclog("Write keyboard %02X %08X\n",v,PC);
        switch (v)
        {
        case KBD_CMD_RESET:
                kbdreset=2;
                kcallback=4*4;
                break;

        case KBD_CMD_ENABLE:
                kcallback=1*4;
                kbdreset=0;
                kbdcommand = KBD_CMD_ENABLE;
                break;

        default:
                kbdcommand=1;
                kcallback=1*4;
                kbdreset=0;
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
//        printf("Write keyboard enable %02X\n",v);
        if (v && !kbdenable)
        {
                kbdreset=1;
                kcallback=5*4;
        }
	if (v)
		kbdstat |= PS2_CONTROL_ENABLE;
	else
		kbdstat &= ~PS2_CONTROL_ENABLE;
}

static void keyboardsend(unsigned char v)
{
//        rpclog("Keyboard send %02X\n",v);
        kbddata = v;
        iomd.irqb.status |= IOMD_IRQB_KEYBOARD_RX;
        updateirqs();
	kbdstat |= PS2_CONTROL_RX_FULL;
	if (calculateparity(v))
		kbdstat |= PS2_CONTROL_RXPARITY;
	else
		kbdstat &= ~PS2_CONTROL_RXPARITY;
}

void keycallback(void)
{
        PS2Queue *q = &kbdqueue;

        if (kbdreset==1)
        {
                iomd.irqb.status |= IOMD_IRQB_KEYBOARD_TX;
                updateirqs();
                kbdreset=0;
                kbdstat |= PS2_CONTROL_TX_EMPTY;
        }
        else if (kbdreset==2)
        {
                kbdreset=3;
                // keyboardsend(KBD_REPLY_ACK);
                kcallback=500*4;
        }
        else if (kbdreset==3)
        {
                kcallback=0;
                kbdreset=0;
                keyboardsend(KBD_REPLY_POR);
        }
        else switch (kbdcommand)
        {
        case 1:
        case KBD_CMD_ENABLE:
                // rpclog("Send key dataF4\n");
                keyboardsend(KBD_REPLY_ACK);
                kcallback=0;
                kbdcommand=0;
                break;

        case 0xFE:
                // rpclog("Send key dataFE %i %02X\n",kbdpacketpos,kbdpacket[kbdpacketpos]);
                keyboardsend(ps2_read_data(q));
                kcallback=0;
                if (q->count == 0)
                   kbdcommand=0;
/*                {
                        kcallback=0;
                }
                else
                {
                        kcallback=5;
                }*/
//                rpclog("keycallback now %i\n",kcallback);
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
//        printf("Read keyboard stat %02X %07X\n",kbdstat,PC);
        return kbdstat;
}

/**
 * Read from the IOMD PS/2 keyboard Data register
 *
 * @return Value of register
 */
uint8_t
keyboard_data_read(void)
{
        kbdstat &= ~PS2_CONTROL_RX_FULL;
        iomd.irqb.status &= ~IOMD_IRQB_KEYBOARD_RX;
        updateirqs();
        if (kbdcommand==0xFE) kcallback=5*4;
//        rpclog("Read keyboard data %02X %07X\n",iomd.keydat,PC);
        return kbddata;
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
 * Called from within execarm() once the mcallback variable reaches 0.
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
 * This function returns early in 'mousehack' mode, as that uses RISC OS
 * specific SWI interception to provide the mouse data.
 *
 * Called from emulator main loop.
 */
void
mouse_poll(void)
{
	static uint8_t oldmouseb = 0;
	static int oldz = 0;
	int x, y;
	int z, tmpz;
	uint8_t mouseb = mouse_b & 7; /* Allegro */
	uint8_t b;

	/* In mousehack mode all movement data is sent via the SWI callbacks */
	if (mousehack) {
		iomd.mousex = 0;
		iomd.mousey = 0;
		return;
	}

	/* Use the 'Menu' key on the keyboard as a fake Menu mouse click */
	if (key[KEY_MENU] || key[KEY_ALTGR]) {
		mouseb |= 4;
	}

	/* Get the relative X/Y movements since the last call to get_mouse_mickeys() */
	get_mouse_mickeys(&x, &y); /* Allegro */

	/* Get the absolute value of the scroll wheel position */
	z = mouse_z; /* Allegro */

	/* Update quadrature mouse */
	iomd.mousex += x;
	iomd.mousey -= y; /* Allegro and RPC Y axis go in opposite directions */

        if (mousecapture) position_mouse(getxs()>>1,getys()>>1);

        /* Return if not PS/2 mouse */
        if (config.model != CPUModel_ARM7500 && config.model != CPUModel_ARM7500FE) {
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

		if (config.mousetwobutton) {
			/* To help people with only two buttons on their mouse,
			   swap the behaviour of middle and right buttons */
			uint8_t mousel = mouseb & 1;
			uint8_t mouser = (mouseb & 2) >> 1;
			uint8_t mousem = (mouseb & 4) >> 2;

			mouseb = mousel | (mousem << 1) | (mouser << 2);
		}

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

static const int standardkeys[][2]=
{
        {KEY_A,0x1C},{KEY_B,0x32},{KEY_C,0x21},{KEY_D,0x23},
        {KEY_E,0x24},{KEY_F,0x2B},{KEY_G,0x34},{KEY_H,0x33},
        {KEY_I,0x43},{KEY_J,0x3B},{KEY_K,0x42},{KEY_L,0x4B},
        {KEY_M,0x3A},{KEY_N,0x31},{KEY_O,0x44},{KEY_P,0x4D},
        {KEY_Q,0x15},{KEY_R,0x2D},{KEY_S,0x1B},{KEY_T,0x2C},
        {KEY_U,0x3C},{KEY_V,0x2A},{KEY_W,0x1D},{KEY_X,0x22},
        {KEY_Y,0x35},{KEY_Z,0x1A},{KEY_0,0x45},{KEY_1,0x16},
        {KEY_2,0x1E},{KEY_3,0x26},{KEY_4,0x25},{KEY_5,0x2E},
        {KEY_6,0x36},{KEY_7,0x3D},{KEY_8,0x3E},{KEY_9,0x46},
        {KEY_F1,0x05},{KEY_F2,0x06},{KEY_F3,0x04},{KEY_F4,0x0C},
        {KEY_F5,0x03},{KEY_F6,0x0B},{KEY_F7,0x83},{KEY_F8,0x0A},
        {KEY_F9,0x01},{KEY_F10,0x09},{KEY_F11,0x78},{KEY_F12,0x07},
        {KEY_ENTER,0x5A},{KEY_ESC,0x76},{KEY_STOP,0x49},{KEY_COMMA,0x41},
        {KEY_SLASH,0x4A},{KEY_OPENBRACE,0x54},{KEY_CLOSEBRACE,0x5B},
        {KEY_SPACE,0x29},{KEY_TAB,0x0D},{KEY_CAPSLOCK,0x58},{KEY_BACKSPACE,0x66},
        {KEY_MINUS,0x4E},{KEY_EQUALS,0x55},
        {KEY_LSHIFT,0x12},{KEY_LCONTROL,0x14},{KEY_ALT,0x11},{KEY_RSHIFT,0x59},
        {KEY_COLON,0x4C},{KEY_QUOTE,0x52},{KEY_TILDE,0x0E},
        {KEY_ASTERISK,0x7C},{KEY_MINUS_PAD,0x7B},{KEY_PLUS_PAD,0x79},
        {KEY_DEL_PAD,0x71},{KEY_0_PAD,0x70},{KEY_1_PAD,0x69},{KEY_2_PAD,0x72},
        {KEY_3_PAD,0x7A},{KEY_4_PAD,0x6B},{KEY_5_PAD,0x73},{KEY_6_PAD,0x74},
        {KEY_7_PAD,0x6C},{KEY_8_PAD,0x75},{KEY_9_PAD,0x7D},{KEY_NUMLOCK,0x77},
        {KEY_SCRLOCK,0x7E},
        {KEY_SEMICOLON,0x4C},
        /* Workaround apparent Allegro bug where keymappings differ between
           Windows and Linux: results in \ and # being swapped */
#if defined(WIN32) || defined(_WIN32)
        {KEY_BACKSLASH,0x5D},
        {KEY_BACKSLASH2,0x61},
#else
        {KEY_BACKSLASH,0x61},
        {KEY_BACKSLASH2,0x5D},
#endif
        {-1,-1}
};

static int findkey(int c)
{
        int d = 0;

        while (standardkeys[d][0] != -1) {
                if (standardkeys[d][0] == c) {
                        return d;
                }
                d++;
        }
        return -1;
}

static const int extendedkeys[][3]=
{
        {KEY_INSERT,0xE0,0x70},{KEY_HOME,0xE0,0x6C},{KEY_PGUP,0xE0,0x7D},
        {KEY_DEL,0xE0,0x71},{KEY_END, 0xE0,0x69},{KEY_PGDN,0xE0,0x7A},
        {KEY_UP,0xE0,0x75},{KEY_LEFT,0xE0,0x6B},{KEY_DOWN,0xE0,0x72},
        {KEY_RIGHT,0xE0,0x74},{KEY_SLASH_PAD,0xE0,0x4A},{KEY_ENTER_PAD,0xE0,0x5A},
        {KEY_ALTGR,0xE0,0x11},{KEY_RCONTROL,0xE0,0x14},
        {-1,-1,-1}
};

static int findextkey(int c)
{
        int d = 0;

        while (extendedkeys[d][0] != -1) {
                if (extendedkeys[d][0] == c) {
                        return d;
                }
                d++;
        }
        return -1;
}

void pollkeyboard(void)
{
        int c;

        for (c = 0; c < 128; c++) {
                int idx;

                if (key[c] == keys2[c]) {
                        /* no change in state */
                        continue;
                }

                keys2[c] = key[c];
                if ((idx = findkey(c)) != -1) {
                        if (!keys2[c]) {
                                /* key-up modifier */
                                ps2_queue(&kbdqueue, 0xF0);
                        }
                        /* 1-byte scan code */
                        ps2_queue(&kbdqueue, standardkeys[idx][1]);

                } else if ((idx = findextkey(c)) != -1) {
                        /* first of 2-byte scan code  */
                        ps2_queue(&kbdqueue, extendedkeys[idx][1]);
                        if (!keys2[c]) {
                                /* key-up modifier */
                                ps2_queue(&kbdqueue, 0xF0);
                        }
                        /* second of 2-byte scan code  */
                        ps2_queue(&kbdqueue, extendedkeys[idx][2]);

                } else if (c == KEY_PAUSE) {
                        /* Break has 8-byte key-down code, and no key-up */
                        ps2_queue(&kbdqueue, 0xe1);
                        ps2_queue(&kbdqueue, 0x14);
                        ps2_queue(&kbdqueue, 0x77);
                        ps2_queue(&kbdqueue, 0xe1);
                        ps2_queue(&kbdqueue, 0xf0);
                        ps2_queue(&kbdqueue, 0x14);
                        ps2_queue(&kbdqueue, 0xf0);
                        ps2_queue(&kbdqueue, 0x77);
                } else {
                        /* unhandled key */
                        continue;
                }

                kcallback = 20;
                kbdcommand = 0xFE;
                return;
        }
}

/* Mousehack functions */

static short ml,mr,mt,mb;
static int activex[5],activey[5];

/**
 * Get the x and y coords in native and OS units.
 *
 * @param x
 * @param y
 * @param osx
 * @param osy
 */
static void
mouse_get_osxy(int *x, int *y, int *osx, int *osy)
{
        assert(mousehack);

        *osy=(getys()<<1)-(mouse_y<<1);
        if (*osy<mt) *osy=mt;
        if (*osy>mb) *osy=mb;
        *y=((getys()<<1)-*osy)>>1;

        *osx=mouse_x<<1;
        if (*osx>mr) *osx=mr;
        if (*osx<ml) *osx=ml;
        *x=*osx>>1;

        if (((mouse_y != *y) || (mouse_x != *x)) && mousehack)
        {
                /* Restrict the pointer to the bounding box, unless the 
                   box is greater than or equal to the full screen size */
                if ((ml > 0) || (mr <= ((getxs()-1)<<1)) ||
                    (mt > 0) || (mb <= ((getys()-1)<<1)))
                {
                        position_mouse(*x,*y);
                }
        }
}

/**
 * OS_Word 21, 4 Read unbuffered mouse position
 *
 * Called from arm.c/ArmDynarec.c SWI handler
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

        writememb(a+1,osy&0xFF);
        writememb(a+2,(osy>>8)&0xFF);
        writememb(a+3,osx&0xFF);
        writememb(a+4,(osx>>8)&0xFF);
}

/**
 * Return the X/Y active point of the cursor.
 *
 * Used by VIDC to determine if the cursor has moved since its last redraw.
 *
 * @param x Filled in with X coordinate of cursor
 * @param y Filled in with Y coordinate of cursor
 */
void
mouse_hack_get_pos(int *x, int *y)
{
        int osx;
        int osy;

        assert(mousehack);

	if (cursor_linked) {
		/* Cursor is at current mouse pointer pos */
		mouse_get_osxy(x, y, &osx, &osy);

		*x -= activex[point];
		*y -= activey[point];
	} else {
		/* Cursor has been detached from mouse pointer and is independent and not moving */
		*x = cursor_unlinked_x;
		*y = cursor_unlinked_y;
	}
}

/**
 * OS_Word 21, 0 Define pointer size, shape and active point
 *
 * Called from arm.c/ArmDynarec.c SWI handler
 *
 * @param a Address of OS_Word 21 parameter block
 */
void
mouse_hack_osword_21_0(uint32_t a)
{
        int num=readmemb(a+1);

        assert(mousehack);

	/* Reject any pointer shapes not in range 0-4 */
        if (num > 4)
		return;

        activex[num]=readmemb(a+4);
        activey[num]=readmemb(a+5);
}

/**
 * OS_Byte 106 Select pointer / activate mouse
 *
 * Called from arm.c/ArmDynarec.c SWI handler
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

	point = a & 0x7f; /* Obtain pointer number (range 0 to 4) */

	/* Bit 7 =  Unlink visible pointer from mouse */
	if (a & 0x80) {
		/* Remember the location of the cursor as the mouse pointer is now independent */
		mouse_hack_get_pos(&cursor_unlinked_x, &cursor_unlinked_y);

		cursor_linked = 0;
	} else {
		cursor_linked = 1;
	}

	/* point should now contain selected number 1-4 or 0 if turned off */
	assert(point >= 0 && point <= 4);
}

/**
 * mousehack handler of OS_Mouse SWI
 *
 * Fill in SWI return values with x, y and button info
 *
 * Called from arm.c/ArmDynarec.c SWI handler on OS_Mouse
 */
void
mouse_hack_osmouse(void)
{
        int32_t temp;

        assert(mousehack);

        temp = (getys() << 1) - (mouse_y << 1); /* Allegro */
        if (temp<mt) temp=mt;
        if (temp>mb) temp=mb;
        armregs[1] = temp;                      /* R1 = mouse y coordinate */

        temp = mouse_x << 1;                    /* Allegro */
        if (temp>mr) temp=mr;
        if (temp<ml) temp=ml;
        armregs[0] = temp;                      /* R0 = mouse x coordinate */

        temp=0;
	if (mouse_b & 1) temp |= 4;             /* Left button */
	if (config.mousetwobutton) {
		/* To help people with only two buttons on their mouse, swap
		   the behaviour of middle and right buttons */
		if (mouse_b & 2) temp |= 2;             /* Middle button */
		if (mouse_b & 4) temp |= 1;             /* Right button */
		if (key[KEY_MENU] || key[KEY_ALTGR]) temp |= 1;
	} else {
		if (mouse_b & 2) temp |= 1;             /* Right button */
		if (mouse_b & 4) temp |= 2;             /* Middle button */
		if (key[KEY_MENU] || key[KEY_ALTGR]) temp |= 2;
	}
        armregs[2] = temp;                      /* R2 = mouse buttons */

        armregs[3] = 0;                         /* R3 = time of button change */
}

/**
 * OS_Word 21, 1 Define Mouse Coordinate bounding box
 *
 * Called from arm.c/ArmDynarec.c SWI handler
 *
 * @param a Address of OS_Word 21 parameter block
 */
void
mouse_hack_osword_21_1(uint32_t a)
{
        assert(mousehack);

        ml=readmemb(a+1)|(readmemb(a+2)<<8);
        mt=readmemb(a+3)|(readmemb(a+4)<<8);
        mr=readmemb(a+5)|(readmemb(a+6)<<8);
        mb=readmemb(a+7)|(readmemb(a+8)<<8);
}
