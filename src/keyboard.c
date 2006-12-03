/*RPCemu v0.5 by Tom Walker
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

#include <allegro.h>
#include "rpcemu.h"

int xs,ys;

int mousecapture;
int timetolive;
int kbdenable=0,kbdreset;
unsigned char kbdstat;
unsigned char kbdpacket[3]={0,0,0};
int kbdpacketsize=0,kbdpacketpos=0;
unsigned char kbdcommand;
int keys2[128];

int msenable=0,msreset;
unsigned char msstat,mscommand;
unsigned char mspacket[3]={0,0,0};
int mspacketpos=0;
unsigned char lastcommand;
int mousepoll=0;

int calculateparity(unsigned char v)
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
        msenable=0;
        mcallback=0;
        msreset=0;
        msstat=0;
        for (c=0;c<128;c++)
            keys2[c]=0;
}

void writekbd(uint8_t v)
{
//        rpclog("Write keyboard %02X %08X\n",v,PC);
        switch (v)
        {
                case 0xFF:
                kbdreset=2;
                kcallback=4*4;
                break;
                case 0xF4:
                kcallback=1*4;
                kbdreset=0;
                kbdcommand=0xF4;
                break;
                default:
                kbdcommand=1;
                kcallback=1*4;
                kbdreset=0;
        }
}

void writekbdenable(int v)
{
//        printf("Write keyboard enable %02X\n",v);
        if (v && !kbdenable)
        {
                kbdreset=1;
                kcallback=5*4;
        }
        if (v) kbdstat|=8;
        else   kbdstat&=~8;
}

void keyboardsend(unsigned char v)
{
//        rpclog("Keyboard send %02X\n",v);
        iomd.keydat=v;
        iomd.statb|=0x80;
        updateirqs();
        kbdstat|=0x20;
        if (calculateparity(v)) kbdstat|=4;
        else                    kbdstat&=~4;
}

void keycallback(void)
{
        if (kbdreset==1)
        {
                iomd.statb|=0x40;
                updateirqs();
                kbdreset=0;
                kbdstat|=0x80;
        }
        else if (kbdreset==2)
        {
                kbdreset=3;
//                keyboardsend(0xFA);
                kcallback=500*4;
        }
        else if (kbdreset==3)
        {
                kcallback=0;
                kbdreset=0;
                keyboardsend(0xAA);
        }
        else switch (kbdcommand)
        {
                case 1:
                case 0xF4:
//                        rpclog("Send key dataF4\n");
                keyboardsend(0xFA);
                kcallback=0;
                kbdcommand=0;
                break;
                case 0xFE:
//                        rpclog("Send key dataFE\n");
                keyboardsend(kbdpacket[kbdpacketpos++]);
                kcallback=0;
                if (kbdpacketpos>=kbdpacketsize)
                   kbdcommand=0;
/*                {
                        kcallback=0;
                }
                else
                {
                        kcallback=5;
                }*/
//                printf("keycallback now %i\n",kcallback);
                break;
        }
}

unsigned char getkeyboardstat(void)
{
//        printf("Read keyboard stat %02X %07X\n",kbdstat,PC);
        return kbdstat;
}

unsigned char readkeyboarddata(void)
{
        kbdstat&=~0x20;
        iomd.statb&=~0x80;
        updateirqs();
        if (kbdcommand==0xFE) kcallback=5*4;
//        rpclog("Read keyboard data %02X %07X\n",iomd.keydat,PC);
        return iomd.keydat;
}

void writemsenable(int v)
{
//        printf("Write mouse enable %02X\n",v);
        v&=8;
//        timetolive=250;
        if (v)// && !msenable)
        {
                msreset=1;
                mcallback=20;
        }
        if (v) msstat|=8;
        else   msstat&=~8;
}

int msincommand=0;
int justsent=0;
void writems(unsigned char v)
{
//        printf("Write mouse %02X %08X  %02X\n",v,PC,msincommand);
/*        if (v==0xFE)
        {
                timetolive=50;
        }*/
        msstat=(msstat&0x3F)|0x40;
        iomd.statd&=~2;
        updateirqs();
        justsent=1;
        if (msincommand)
        {
                switch (msincommand)
                {
                        case 0xE8:
                        case 0xF3:
                        mspacketpos=1;
                        mcallback=200;
                        return;
                }
        }
        else
        {
                mscommand=v;
                switch (v)
                {
                        case 0xFF:
                        msreset=2;
                        mcallback=200;
                        mousepoll=0;
                        break;
                        case 0xFE:
                        msreset=0;
                        mcallback=1500;
                        mspacketpos=0;
                        break;
                        case 0xF4:
                        mcallback=200;
                        msreset=0;
                        break;
                        case 0xF3:
                        msincommand=0xF3;
                        msreset=0;
                        mcallback=200;
                        mspacketpos=0;
                        break;
                        case 0xF2:
                        msincommand=1;
                        msreset=0;
                        mcallback=200;
                        mspacketpos=0;
                        break;
                        case 0xE8:
                        msincommand=0xE8;
                        msreset=0;
                        mcallback=200;
                        mspacketpos=0;
                        break;
                        case 0xE7:
                        msincommand=0xE7;
                        msreset=0;
                        mcallback=200;
                        mspacketpos=0;
                        break;
                        case 0xE6:
                        msincommand=0xE6;
                        msreset=0;
                        mcallback=200;
                        mspacketpos=0;
                        break;
                        default:
                        error("Bad mouse command %02X\n",v);
                        exit(-1);
                }
        }
}

unsigned char getmousestat(void)
{
//        printf("Read mouse status %02X\n",msstat);
        return msstat;
}

unsigned char readmousedata(void)
{
        unsigned char temp=iomd.msdat;
        msstat&=~0x20;
        iomd.statd&=~0x1;
        updateirqs();
        if (mspacketpos<3 && mscommand==0xFE) mcallback=300;
//        printf("Read mouse data %02X\n",iomd.msdat);
//        timetolive=500;
        iomd.msdat=0;
        return temp;
}

void mousesend(unsigned char v)
{
        iomd.msdat=v;
        iomd.statd|=1;
        updateirqs();
        msstat|=0x20;
//        printf("Send data %02X\n",v);
        if (calculateparity(v)) msstat|=4;
        else                    msstat&=~4;
//        timetolive=250;
}

void mscallback(void)
{
        msstat=(msstat&0x3F)|0x80;
        if (justsent)
        {
                iomd.statd|=2;
                updateirqs();
                justsent=0;
        }
        if (msreset==1)
        {
                iomd.statd|=2;
                updateirqs();
                msreset=3;
                msstat|=0x80;
                mcallback=200;
        }
        else if (msreset==2)
        {
                msreset=3;
                mousesend(0xFA);
                mcallback=400;
        }
        else if (msreset==3)
        {
                mcallback=200;
                mousesend(0xAA);
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
                case 0xE8:
                case 0xF3:
//                printf("%02X callback %i\n",mscommand,mspacketpos);
                mousesend(0xFA);
                if (mspacketpos) msincommand=0;
                break;
                case 0xF2:
                if (msincommand==1)
                {
                        msincommand=2;
                        mcallback=200;
                        mousesend(0xFA);
                }
                else
                {
                        mousesend(0);
                        msincommand=0;
                }
                break;
                case 0xF4:
                mousesend(0xFA);
                mousepoll=1;
                break;
                case 0xFE:
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
                case 0xE6:
                case 0xE7:
                mousesend(0xFA);
                msincommand=0;
                break;
        }
}

unsigned char oldmouseb=0;

void pollmouse()
{
        int x,y;
        unsigned char mouseb=mouse_b&7;
        if (mousehack)
        {
                iomd.mousex=iomd.mousey=0;
                return;
        }
        if (key[KEY_MENU]) mouseb|=4;
        get_mouse_mickeys(&x,&y);
        iomd.mousex+=x;
        iomd.mousey+=y;
        if (mousecapture) position_mouse(getxs()>>1,getys()>>1);
        if (model) return;
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
        mcallback=400;
        mscommand=0xFE;
        mspacketpos=0;
}

int standardkeys[][2]=
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
        {KEY_MINUS,0x4E},{KEY_EQUALS,0x55},{KEY_BACKSLASH,0x5D},
        {KEY_LSHIFT,0x12},{KEY_LCONTROL,0x14},{KEY_ALT,0x11},{KEY_RSHIFT,0x59},
        {KEY_SEMICOLON,0x4C},{KEY_QUOTE,0x52},{KEY_TILDE,0x0E},
        {KEY_ASTERISK,0x7C},{KEY_MINUS_PAD,0x7B},{KEY_PLUS_PAD,0x79},
        {KEY_DEL_PAD,0x71},{KEY_0_PAD,0x70},{KEY_1_PAD,0x69},{KEY_2_PAD,0x72},
        {KEY_3_PAD,0x7A},{KEY_4_PAD,0x6B},{KEY_5_PAD,0x73},{KEY_6_PAD,0x74},
        {KEY_7_PAD,0x6C},{KEY_8_PAD,0x75},{KEY_9_PAD,0x7D},{KEY_NUMLOCK,0x77},
        {KEY_SCRLOCK,0x7E},{KEY_BACKSLASH2,0x5C},
        {-1,-1}
};

int findkey(int c)
{
        int d=0;
        while (1)
        {
                if (standardkeys[d][0]==c || standardkeys[d][0]==-1)
                {
                        return standardkeys[d][1];
                }
                d++;
        }
        return -1;
}

unsigned char kbdtemp;

int extendedkeys[][3]=
{
        {KEY_INSERT,0xE0,0x70},{KEY_HOME,0xE0,0x6C},{KEY_PGUP,0xE0,0x7D},
        {KEY_DEL,0xE0,0x71},{KEY_END, 0xE0,0x69},{KEY_PGDN,0xE0,0x7A},
        {KEY_UP,0xE0,0x75},{KEY_LEFT,0xE0,0x6B},{KEY_DOWN,0xE0,0x72},
        {KEY_RIGHT,0xE0,0x74},{KEY_SLASH_PAD,0xE0,0x4A},{KEY_ENTER_PAD,0xE0,0x5A},
        {KEY_ALTGR,0xE0,0x11},{KEY_RCONTROL,0xE0,0x14},
        {-1,-1,-1}
};

int findextkey(int c)
{
        int d=0;
        while (1)
        {
                if (extendedkeys[d][0]==c || extendedkeys[d][0]==-1)
                {
                        kbdtemp=extendedkeys[d][1];
                        return extendedkeys[d][2];
                }
                d++;
        }
        return -1;
}

void pollkeyboard()
{
        int c;
        int temp;
        for (c=0;c<128;c++)
        {
                if (key[c]!=keys2[c])
                {
                        keys2[c]=key[c];
                        temp=findkey(c);
//                        printf("Found key %i %02X\n",c,temp);
                        if (temp!=-1)
                        {
                                if (!keys2[c])
                                {
                                        kbdpacket[0]=0xF0;
                                        kbdpacket[1]=temp;
                                        kbdpacketsize=2;
                                }
                                else
                                {
                                        kbdpacket[0]=temp;
                                        kbdpacketsize=1;
                                }
                                kbdpacketpos=0;
                                kcallback=20;
                                kbdcommand=0xFE;
//                                rpclog("Sending packet %02X %02X %i\n",kbdpacket[0],kbdpacket[1],kbdpacketsize);
                                return;
                        }
                        else
                        {
                                temp=findextkey(c);
//                                printf("Found extended key %i %02X %02X\n",c,temp,kbdtemp);
                                if (temp!=-1)
                                {
                                        if (!keys2[c])
                                        {
                                                kbdpacket[0]=kbdtemp;
                                                kbdpacket[1]=0xF0;
                                                kbdpacket[2]=temp;
                                                kbdpacketsize=3;
                                        }
                                        else
                                        {
                                                kbdpacket[0]=kbdtemp;
                                                kbdpacket[1]=temp;
                                                kbdpacketsize=2;
                                        }
                                        kbdpacketpos=0;
                                        kcallback=20;
                                        kbdcommand=0xFE;
//                                        rpclog("Sending packet %02X %02X %02X %i\n",kbdpacket[0],kbdpacket[1],kbdpacket[2],kbdpacketsize);
                                        return;
                                }
                        }
                }
        }
}

short ml,mr,mt,mb;
int activex[5],activey[5];
void doosmouse()
{
        short temp;
        return;
        if (!mousehack) return;
//        printf("doosmouse\n");
//        if (!mousehack || fullscreen) return;
        temp=(getys()<<1)-((mouse_y/*-offsety*/)<<1);
//        if (temp<0) temp=0;
        if (temp<mt) temp=mt;
        if (temp>mb) temp=mb;
//        ymouse=temp;
//        writememl(0x5B8,temp);
        writememl(0x5A4,temp);
        temp=(mouse_x/*-offsetx*/)<<1;
        if (temp>mr) temp=mr;
        if (temp<ml) temp=ml;
//        xmouse=temp;
//        writememl(0x5B4,temp);
        writememl(0x5A0,temp);
/*        *armregs[0]=mouse_x;
        if (mouse_x>639) *armregs[0]=639;
        *armregs[1]=mouse_y>>1;
        temp=0;
        if (mouse_b&1) temp|=1;
        if (mouse_b&2) temp|=4;
        if (mouse_b&4) temp|=2;
        if (key[KEY_MENU]) temp|=2;
        *armregs[2]=temp;
        *armregs[3]=0;*/
}

void setmousepos(uint32_t a)
{
        unsigned short temp,temp2;
//        printf("setmousepos\n");
        temp=readmemb(a+1)|(readmemb(a+2)<<8);
        temp=temp>>1;
        temp2=readmemb(a+3)|(readmemb(a+4)<<8);
        temp2=((mb+1)-temp2)>>1;
//        position_mouse(temp,temp2);
}

void getunbufmouse(uint32_t a)
{
        short temp;
//        return;
//        printf("getunbufmouse\n");
        temp=(getys()<<1)-((mouse_y/*-offsety*/)<<1);
        if (temp<mt) temp=mt;
        if (temp>mb) temp=mb;
        writememb(a+1,temp&0xFF);
        writememb(a+2,(temp>>8)&0xFF);
        temp=(mouse_x/*-offsetx*/)<<1;
        if (temp>mr) temp=mr;
        if (temp<ml) temp=ml;
        writememb(a+3,temp&0xFF);
        writememb(a+4,(temp>>8)&0xFF);
}

int point=0;
void getmousepos(int *x, int *y)
{
        short temp;
        temp=(getys()<<1)-(mouse_y<<1);
//        printf("Mouse Y - %i %i  ",mouse_y,temp);
        if (temp<mt) temp=mt;
        if (temp>mb) temp=mb;
//        printf("%i %i  %i  ",mt,mb,temp);
        temp=((getys()<<1)-temp)>>1;
//        printf("%i %i\n",temp,(int)temp-activey[point]);
        *y=(int)temp-activey[point];
        temp=mouse_x<<1;
        if (temp>mr) temp=mr;
        if (temp<ml) temp=ml;
        temp>>=1;
        *x=(int)temp-activex[point];
}
        
void setpointer(uint32_t a)
{
        int num=readmemb(a+1);
        if (num>4) return;
        activex[num]=readmemb(a+4);
        activey[num]=readmemb(a+5);
//        printf("setpointer %i %i %i\n",num,activex,activey);
}

void osbyte106(uint32_t a)
{
        point=a;
        if (point&0x80) point=0;
        if (point>4) point=0;
//        printf("osbyte 106 %i %i\n",a,point);
}
void getosmouse()
{
        long temp;
        temp=(getys()<<1)-(mouse_y<<1);
        if (temp<mt) temp=mt;
        if (temp>mb) temp=mb;
        armregs[1]=temp;
        temp=mouse_x<<1;
        if (temp>mr) temp=mr;
        if (temp<ml) temp=ml;
        armregs[0]=temp;
        temp=0;
        if (mouse_b&1) temp|=4;
        if (mouse_b&2) temp|=1;
        if (mouse_b&4) temp|=2;
        if (key[KEY_MENU]) temp|=2;
        armregs[2]=temp;
        armregs[3]=0;
}

void setmouseparams(uint32_t a)
{
        ml=readmemb(a+1)|(readmemb(a+2)<<8);
        mt=readmemb(a+3)|(readmemb(a+4)<<8);
        mr=readmemb(a+5)|(readmemb(a+6)<<8);
        mb=readmemb(a+7)|(readmemb(a+8)<<8);
//        printf("Mouse params %04X %04X %04X %04X\n",ml,mr,mt,mb);
//        fputs(bigs,olog);
}

void resetmouse()
{
        ml=mt=0;
        mr=0x4FF;
        mb=0x3FF;
}
