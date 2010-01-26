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
static uint8_t kbdstat;		/**<  PS/2 control register for the keyboard */
static unsigned char kbdcommand;
static int keys2[128];
static PS2Queue kbdqueue;

static int msenable, msreset;
static uint8_t msstat;		/**<  PS/2 control register for the mouse */
static unsigned char mscommand;
static unsigned char mspacket[3];
static int mspacketpos;
static int mousepoll;

static int msincommand;
static int justsent;

static int point;

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
        msenable=0;
        mcallback=0;
        msreset=0;
        msstat=0;
        for (c=0;c<128;c++)
            keys2[c]=0;
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
        iomd.keydat=v;
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

uint8_t
keyboard_status_read(void)
{
//        printf("Read keyboard stat %02X %07X\n",kbdstat,PC);
        return kbdstat;
}

uint8_t
keyboard_data_read(void)
{
        kbdstat &= ~PS2_CONTROL_RX_FULL;
        iomd.irqb.status &= ~IOMD_IRQB_KEYBOARD_RX;
        updateirqs();
        if (kbdcommand==0xFE) kcallback=5*4;
//        rpclog("Read keyboard data %02X %07X\n",iomd.keydat,PC);
        return iomd.keydat;
}

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

void
mouse_data_write(uint8_t v)
{
//        printf("Write mouse %02X %08X  %02X\n",v,PC,msincommand);
        /* Set BUSY flag, clear EMPTY flag */
        msstat = (msstat & 0x3f) | PS2_CONTROL_TX_BUSY;

	mouse_irq_tx_lower();

        justsent=1;
        if (msincommand)
        {
                switch (msincommand)
                {
                case AUX_SET_RES:
                case AUX_SET_SAMPLE:
                        mspacketpos=1;
                        mcallback=20;
                        return;
                }
        }
        else
        {
                mscommand=v;
                switch (v)
                {
                case AUX_RESET:
                        msreset=2;
                        mcallback=20;
                        mousepoll=0;
                        break;
                case AUX_RESEND:
                        msreset=0;
                        mcallback=150;
                        mspacketpos=0;
                        break;
                case AUX_ENABLE_DEV:
                        mcallback=20;
                        msreset=0;
                        break;
                case AUX_SET_SAMPLE:
                        msincommand = AUX_SET_SAMPLE;
                        msreset=0;
                        mcallback=20;
                        mspacketpos=0;
                        break;
                case AUX_GET_TYPE:
                        msincommand=1;
                        msreset=0;
                        mcallback=20;
                        mspacketpos=0;
                        break;
                case AUX_SET_RES:
                        msincommand = AUX_SET_RES;
                        msreset=0;
                        mcallback=20;
                        mspacketpos=0;
                        break;
                case AUX_SET_SCALE21:
                        msincommand = AUX_SET_SCALE21;
                        msreset=0;
                        mcallback=20;
                        mspacketpos=0;
                        break;
                case AUX_SET_SCALE11:
                        msincommand = AUX_SET_SCALE11;
                        msreset=0;
                        mcallback=20;
                        mspacketpos=0;
                        break;
                default:
                        error("Bad mouse command %02X\n",v);
                        dumpregs();
                        exit(-1);
                }
        }
}

uint8_t
mouse_status_read(void)
{
//        printf("Read mouse status %02X\n",msstat);
        return msstat;
}

uint8_t
mouse_data_read(void)
{
        unsigned char temp=iomd.msdat;

        msstat &= ~PS2_CONTROL_RX_FULL;

        mouse_irq_rx_lower();

        if (mspacketpos < 3 && mscommand == AUX_RESEND) {
                mcallback = 20;
        }
//        printf("Read mouse data %02X\n",iomd.msdat);
        iomd.msdat=0;
        return temp;
}

static void mousesend(unsigned char v)
{
        iomd.msdat=v;

        mouse_irq_rx_raise();

        msstat |= PS2_CONTROL_RX_FULL;
//        printf("Send data %02X\n",v);
	if (calculateparity(v))
		msstat |= PS2_CONTROL_RXPARITY;
	else
		msstat &= ~PS2_CONTROL_RXPARITY;
}

void mscallback(void)
{
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
                mousesend(AUX_ACK);
                mcallback=40;
        }
        else if (msreset==3)
        {
                mcallback=20;
                mousesend(AUX_TEST_OK);
                msreset=4;
        }
        else if (msreset==4)
        {
                msreset=0;
                mousesend(0);
                mcallback=0;
        }
        else switch (mscommand)
        {
        case AUX_SET_RES:
        case AUX_SET_SAMPLE:
//                printf("%02X callback %i\n",mscommand,mspacketpos);
                mousesend(AUX_ACK);
                if (mspacketpos) msincommand=0;
                break;
        case AUX_GET_TYPE:
                if (msincommand==1)
                {
                        msincommand=2;
                        mcallback=20;
                        mousesend(AUX_ACK);
                }
                else
                {
                        mousesend(0);
                        msincommand=0;
                }
                break;
        case AUX_ENABLE_DEV:
                mousesend(AUX_ACK);
                mousepoll=1;
                break;
        case AUX_RESEND:
                mousesend(mspacket[mspacketpos++]);
                if (mspacketpos>=3)
                {
                        mcallback=0;
                }
/*                else
                {
                        mcallback=300;
                }                                       */
//                printf("Callback now %i\n",mcallback);
                break;
        case AUX_SET_SCALE11:
        case AUX_SET_SCALE21:
                mousesend(AUX_ACK);
                msincommand=0;
                break;
        }
}

void pollmouse(void)
{
        static unsigned char oldmouseb = 0;
        int x,y;
        unsigned char mouseb=mouse_b&7;
        if (mousehack)
        {
                iomd.mousex=iomd.mousey=0;
                return;
        }
        if (key[KEY_MENU]) mouseb|=4;
//		if (key[KEY_Z]) mouseb|=4;
//		if (key[KEY_X]) mouseb|=2;
        get_mouse_mickeys(&x,&y);
        iomd.mousex+=x;
        iomd.mousey+=y;
//        rpclog("Poll mouse %i %i %i %i\n",x,y,iomd.mousex,iomd.mousey);
        if (mousecapture) position_mouse(getxs()>>1,getys()>>1);

        /* Return if not PS/2 mouse */
        if (config.model != CPUModel_ARM7500 && config.model != CPUModel_ARM7500FE) {
                return;
        }

        if (!mousepoll) return;
        if (!x && !y && (mouseb==oldmouseb)) return;
        oldmouseb=mouseb;
        if (x<-256) x=-256;
        if (x>255) x=255;
        if (y<-256) y=-256;
        if (y>255) y=255;
        y^=0xFFFFFFFF;
        y++;
        mspacket[0]=(mouseb&7)|8;
        if (x&0x100) mspacket[0]|=0x10;
        if (y&0x100) mspacket[0]|=0x20;
        mspacket[1]=x&255;
        mspacket[2]=y&255;
        mcallback=20;
        mscommand = AUX_RESEND;
        mspacketpos=0;
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

/* Gets the x and y coords in native and OS units */
static void getmouseosxy(int *x, int *y, int *osx, int *osy)
{
        assert(mousehack);

        *osy=(getys()<<1)-(mouse_y<<1);
//        printf("Mouse Y - %i %i  ",mouse_y,*osy);
        if (*osy<mt) *osy=mt;
        if (*osy>mb) *osy=mb;
//        printf("%i %i  %i  ",mt,mb,*osy);
        *y=((getys()<<1)-*osy)>>1;
//        printf("%i\n",*y);

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

        getmouseosxy(&x,&y,&osx,&osy);

        writememb(a+1,osy&0xFF);
        writememb(a+2,(osy>>8)&0xFF);
        writememb(a+3,osx&0xFF);
        writememb(a+4,(osx>>8)&0xFF);
}

void getmousepos(int *x, int *y)
{
        int osx;
        int osy;

        assert(mousehack);

        getmouseosxy(x,y,&osx,&osy);

        *y-=activey[point];
        *x-=activex[point];
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

        point = a;
        /* Bit 7 =  Unlink visible pointer from mouse */
        if (point & 0x80)
		point = 0;
        if (point > 4)
		point = 0;

        /* point should now contain selected number 1-4 or
           0 if turned off */
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
        uint32_t temp;

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
        if (mouse_b & 1) temp |= 4;             /* Allegro */
        if (mouse_b & 2) temp |= 1;             /* Allegro */
        if (mouse_b & 4) temp |= 2;             /* Allegro */
        if (key[KEY_MENU]) temp|=2;
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
