#include <stdint.h>
#include <allegro.h>
#include "rpcemu.h"
#include "vidc20.h"
#include "sound.h"
#include "iomd.h"
#include "82c711.h"
#include "ide.h"
#include "cdrom-iso.h"

extern void ioctl_init(void);

/* Indexes into the configuregui array */
//#define CONF_BOX                   0
//#define CONF_OK                    1
//#define CONF_CANCEL                2
//#define CONF_LABEL_CPU             3
#define CONF_ARM7500               4
#define CONF_ARM610                5
#define CONF_ARM710                6
#define CONF_SA110                 7
//#define CONF_LABEL_RAM             8
#define CONF_RAM_4                 9
#define CONF_RAM_8                10
#define CONF_RAM_16               11
#define CONF_RAM_32               12
#define CONF_RAM_64               13
#define CONF_RAM_128              14
//#define CONF_LABEL_VRAM           15
#define CONF_VRAM_0               16
#define CONF_VRAM_2               17
#define CONF_SOUND                18
//#define CONF_LABEL_HZ             19
#define CONF_HZ_SLIDER            20
#define CONF_HZ_TEXT              21

static DIALOG configuregui[];

static int menuexit(void)
{
        quited=1;
        return D_CLOSE;
}

static int menureset(void)
{
        resetrpc();
        return D_CLOSE;
}

static MENU filemenu[]=
{
        {"&Reset",menureset,NULL,0,NULL},
        {"E&xit",menuexit,NULL,0,NULL},
        {NULL,NULL,NULL,0,NULL}
};

static int menuld0(void)
{
        char fn[260];
        int ret;//,c;
        int xsize=SCREEN_W-32,ysize=SCREEN_H-64;
        memcpy(fn,discname[0],260);
        ret=file_select_ex("Please choose a disc image",fn,"ADF",260,xsize,ysize);
        if (ret)
        {
                saveadf(discname[0], 0);
                strcpy(discname[0],fn);
                loadadf(discname[0], 0);
        }
        return D_EXIT;
}

static int menuld1(void)
{
        char fn[260];
        int ret;//,c;
        int xsize=SCREEN_W-32,ysize=SCREEN_H-64;
        memcpy(fn,discname[1],260);
        ret=file_select_ex("Please choose a disc image",fn,"ADF",260,xsize,ysize);
        if (ret)
        {
                saveadf(discname[1], 1);
                strcpy(discname[1],fn);
                loadadf(discname[1], 1);
        }
        return D_EXIT;
}

static MENU discmenu[]=
{
        {"Load drive :&0...",menuld0,NULL,0,NULL},
        {"Load drive :&1...",menuld1,NULL,0,NULL},
        {NULL,NULL,NULL,0,NULL}
};

static int cddisabled(void)
{
	int res;
	if (cdromenabled)
	{
		res=alert("This will reset RPCemu!","Okay to continue?",NULL,"OK","Cancel",0,0);
		if (res==1)
		{
			cdromenabled=0;
			resetrpc();
		}
	}
	return D_CLOSE;
}

static int cdempty(void)
{
	int res;
	if (!cdromenabled)
	{
		res=alert("This will reset RPCemu!","Okay to continue?",NULL,"OK","Cancel",0,0);
		if (res==1)
		{
			cdromenabled=1;
			resetrpc();
		}
		else		   
	 	   return D_CLOSE;
	}
	atapi->exit();
	iso_init();
	return D_CLOSE;
}

char isoname[512]="";

static int cdisoimage(void)
{
        char fn[260];
        int ret,res;
        int xsize=SCREEN_W-32,ysize=SCREEN_H-64;
	if (!cdromenabled)
	{
		res=alert("This will reset RPCemu!","Okay to continue?",NULL,"OK","Cancel",0,0);
		if (res==1)
		{
			cdromenabled=1;
			resetrpc();
		}
		else
	   	   return D_EXIT;
	}
		        memcpy(fn,isoname,260);
		        ret=file_select_ex("Please choose a disc image",fn,"ISO",260,xsize,ysize);
		        if (ret)
	        	{
		                strcpy(isoname,fn);
				atapi->exit();
                		iso_open(isoname);
		        }
        return D_EXIT;
}

#if defined linux || defined __linux
static int cdioctl(void)
{
	int res;
	if (!cdromenabled)
	{
		res=alert("This will reset RPCemu!","Okay to continue?",NULL,"OK","Cancel",0,0);
		if (res==1)
		{
			cdromenabled=1;
			resetrpc();
		}
		else return D_CLOSE;
	}
	atapi->exit();
	ioctl_init();
	return D_CLOSE;
}
#endif

static MENU cdmenu[]=
{
	{"&Disabled",cddisabled,NULL,0,NULL},
	{"&Empty",cdempty,NULL,0,NULL},
	{"&ISO image...",cdisoimage,NULL,0,NULL},
#if defined linux || defined __linux
	{"&IOCTL",cdioctl,NULL,0,NULL},
#endif
	{NULL,NULL,NULL,0,NULL}
};

static MENU settingsmenu[];

static int menufullscreen(void)
{
        togglefullscreen(!fullscreen);
        settingsmenu[1].flags=(fullscreen)?D_SELECTED:0;
        return D_CLOSE;
}

static int menualt(void)
{
        stretchmode^=1;
        settingsmenu[2].flags=(stretchmode)?D_SELECTED:0;
        return D_CLOSE;
}

static int menublt(void)
{
        skipblits^=1;
        settingsmenu[3].flags=(skipblits)?D_SELECTED:0;
        return D_CLOSE;
}

static int menumouse(void)
{
        mousehackon^=1;
        settingsmenu[4].flags=(mousehackon)?D_SELECTED:0;
        return D_CLOSE;
}

static char hzstring[20];

static int menusettings(void)
{
        int c;
        int changed=0;

        configuregui[CONF_ARM7500].flags = 0;
        configuregui[CONF_ARM610].flags  = 0;
        configuregui[CONF_ARM710].flags  = 0;
        configuregui[CONF_SA110].flags   = 0;

        switch (model) {
        case CPUModel_ARM7500:
                configuregui[CONF_ARM7500].flags = D_SELECTED;
                break;
        case CPUModel_ARM610:
                configuregui[CONF_ARM610].flags = D_SELECTED;
                break;
        case CPUModel_ARM710:
                configuregui[CONF_ARM710].flags = D_SELECTED;
                break;
        case CPUModel_SA110:
                configuregui[CONF_SA110].flags = D_SELECTED;
                break;
        default:
                fprintf(stderr, "Unknown CPU model %d\n", model);
                exit(EXIT_FAILURE);
        }

        configuregui[CONF_RAM_4].flags   = 0;
        configuregui[CONF_RAM_8].flags   = 0;
        configuregui[CONF_RAM_16].flags  = 0;
        configuregui[CONF_RAM_32].flags  = 0;
        configuregui[CONF_RAM_64].flags  = 0;
        configuregui[CONF_RAM_128].flags = 0;
        switch (rammask)
        {
                case 0x01FFFFF: configuregui[CONF_RAM_4].flags   = D_SELECTED; break;
                case 0x03FFFFF: configuregui[CONF_RAM_8].flags   = D_SELECTED; break;
                case 0x07FFFFF: configuregui[CONF_RAM_16].flags  = D_SELECTED; break;
                case 0x0FFFFFF: configuregui[CONF_RAM_32].flags  = D_SELECTED; break;
                case 0x1FFFFFF: configuregui[CONF_RAM_64].flags  = D_SELECTED; break;
                case 0x3FFFFFF: configuregui[CONF_RAM_128].flags = D_SELECTED; break;
        }

        configuregui[CONF_VRAM_0].flags = vrammask     ? 0          : D_SELECTED;
        configuregui[CONF_VRAM_2].flags = vrammask     ? D_SELECTED : 0;
        configuregui[CONF_SOUND].flags  = soundenabled ? D_SELECTED : 0;
        configuregui[CONF_HZ_SLIDER].d2 = (refresh - 20) / 5;
        sprintf(hzstring, "%iHz", refresh);
        configuregui[CONF_HZ_TEXT].dp = hzstring;
        
        position_dialog(configuregui,(SCREEN_W/2)-80,(SCREEN_H/2)-88);
        
        c=popup_dialog(configuregui,1);

        position_dialog(configuregui,-((SCREEN_W/2)-80),-((SCREEN_H/2)-88));
        
        if (c==1)
        {
                CPUModel selected_model = CPUModel_ARM7500;
                int selected_rammask = 0;
                int selected_vrammask = 0;

                if (configuregui[CONF_ARM7500].flags & D_SELECTED) {
                        selected_model = CPUModel_ARM7500;
                } else if (configuregui[CONF_ARM610].flags & D_SELECTED) {
                        selected_model = CPUModel_ARM610;
                } else if (configuregui[CONF_ARM710].flags & D_SELECTED) {
                        selected_model = CPUModel_ARM710;
                } else if (configuregui[CONF_SA110].flags & D_SELECTED) {
                        selected_model = CPUModel_SA110;
                }

                if (model != selected_model) {
                        model = selected_model;
                        changed = 1;
                }

                if (configuregui[CONF_RAM_4].flags & D_SELECTED) {
                        selected_rammask = 0x01FFFFF;
                } else if (configuregui[CONF_RAM_8].flags & D_SELECTED) {
                        selected_rammask = 0x03FFFFF;
                } else if (configuregui[CONF_RAM_16].flags & D_SELECTED) {
                        selected_rammask = 0x07FFFFF;
                } else if (configuregui[CONF_RAM_32].flags & D_SELECTED) {
                        selected_rammask = 0x0FFFFFF;
                } else if (configuregui[CONF_RAM_64].flags & D_SELECTED) {
                        selected_rammask = 0x1FFFFFF;
                } else if (configuregui[CONF_RAM_128].flags & D_SELECTED) {
                        selected_rammask = 0x3FFFFFF;
                }
                if (rammask != selected_rammask) {
                        rammask = selected_rammask;
                        changed = 1;
                }

                if (configuregui[CONF_VRAM_0].flags & D_SELECTED) {
                        selected_vrammask = 0;
                } else if (configuregui[CONF_VRAM_2].flags & D_SELECTED) {
                        selected_vrammask = 0x7FFFFF;
                }
                if (vrammask != selected_vrammask) {
                        vrammask = selected_vrammask;
                        changed  = 1;
                }

                if (changed)
                        resetrpc();
                
                refresh = (configuregui[CONF_HZ_SLIDER].d2 * 5) + 20;
                
                if (soundenabled && !(configuregui[CONF_SOUND].flags & D_SELECTED)) {
                        closesound();
                        soundenabled = 0;
                }
                if (!soundenabled && (configuregui[CONF_SOUND].flags & D_SELECTED)) {
                        initsound();
                        soundenabled = 1;
                }
        }
        return D_CLOSE;
}

static int hzcallback(void *dp3, int d2)
{
        sprintf(hzstring, "%iHz", (d2 * 5) + 20);
        configuregui[CONF_HZ_TEXT].dp = hzstring;
        rectfill(screen,
                 configuregui[CONF_HZ_TEXT].x,
                 configuregui[CONF_HZ_TEXT].y,
                 configuregui[CONF_HZ_TEXT].x + 39,
                 configuregui[CONF_HZ_TEXT].y + 7,
                 0xFFFFFFFF);
        object_message(&configuregui[CONF_HZ_TEXT], MSG_DRAW, 0);

        return 0;
}

static MENU settingsmenu[]=
{
        {"&Settings...",menusettings,NULL,0,NULL},
        {"&Fullscreen mode",menufullscreen,NULL,0,NULL},
        {"&Alternative blitting code",menualt,NULL,0,NULL},
        {"&Blitting optimisation",menublt,NULL,0,NULL},
        {"&Mouse hack",menumouse,NULL,0,NULL},
	{"&CD-ROM",NULL,cdmenu,0,NULL},
        {NULL,NULL,NULL,0,NULL}
};

static MENU mainmenu[]=
{
        {"&File",NULL,filemenu,0,NULL},
        {"&Disc",NULL,discmenu,0,NULL},
        {"&Settings",NULL,settingsmenu,0,NULL},
        {NULL,NULL,NULL,0,NULL}
};

#define CY 0
#define CX 0
/* This array must be kept in sync with the CONF_ defines above */
static DIALOG configuregui[]=
{
        {d_shadow_box_proc, CX,CY-8, 160,176,0,-1,0,0,     0,0,0,0,0}, // 0
        
        {d_button_proc,CX+8,CY+144,64, 16, 0,-1,0,D_EXIT,0,0,"OK",0,0}, // 1
        {d_button_proc,CX+88,CY+144,64, 16, 0,-1,0,D_EXIT,0,0,"Cancel",0,0}, // 2

        {d_text_proc,CX+8,CY-4,40,8,0,-1,0,0,0,0,"CPU :",0,0}, // 3
#ifdef DYNAREC
        {d_radio_proc,CX+8,CY+4,64,16,0,-1,0,D_DISABLED, 0 ,0,"ARM7500",0,0},   // 4
        {d_radio_proc,CX+8,CY+4+16,64,16,0,-1,0,D_DISABLED, 0 ,0,"ARM610",0,0}, // 5
        {d_radio_proc,CX+8,CY+4+32,64,16,0,-1,0,D_DISABLED, 0 ,0,"ARM710",0,0}, // 6
        {d_radio_proc,CX+8,CY+4+48,64,16,0,-1,0,D_DISABLED, 0 ,0,"SA110",0,0},  // 7
#else
        {d_radio_proc,CX+8,CY+4,64,16,0,-1,0,0, 0 ,0,"ARM7500",0,0},   // 4
        {d_radio_proc,CX+8,CY+4+16,64,16,0,-1,0,0, 0 ,0,"ARM610",0,0}, // 5
        {d_radio_proc,CX+8,CY+4+32,64,16,0,-1,0,0, 0 ,0,"ARM710",0,0}, // 6
        {d_radio_proc,CX+8,CY+4+48,64,16,0,-1,0,0, 0 ,0,"SA110",0,0},  // 7
#endif
        {d_text_proc,CX+88,CY-4,40,8,0,-1,0,0,0,0,"RAM :",0,0}, // 8
        {d_radio_proc,CX+88,CY+4,64,16,0,-1,0,0, 1, 0,"4mb",0,0}, // 9
        {d_radio_proc,CX+88,CY+4+16,64,16,0,-1,0,0, 1, 0,"8mb",0,0}, // 10
        {d_radio_proc,CX+88,CY+4+32,64,16,0,-1,0,0, 1, 0,"16mb",0,0}, // 11
        {d_radio_proc,CX+88,CY+4+48,64,16,0,-1,0,0, 1, 0,"32mb",0,0}, // 12
        {d_radio_proc,CX+88,CY+4+64,64,16,0,-1,0,0, 1, 0,"64mb",0,0}, // 13
        {d_radio_proc,CX+88,CY+4+80,64,16,0,-1,0,0, 1, 0,"128mb",0,0}, // 14
        
        {d_text_proc,CX+8,CY+4+72,40,8,0,-1,0,0,0,0,"VRAM :",0,0}, // 15
#ifdef DYNAREC
        {d_radio_proc,CX+8,CY+4+80,64,16,0,-1,0,D_DISABLED, 2, 0,"None",0,0}, // 16
        {d_radio_proc,CX+8,CY+4+96,64,16,0,-1,0,D_DISABLED, 2, 0,"2mb",0,0}, // 17
#else
        {d_radio_proc,CX+8,CY+4+80,64,16,0,-1,0,0, 2, 0,"None",0,0}, // 16
        {d_radio_proc,CX+8,CY+4+96,64,16,0,-1,0,0, 2, 0,"2mb",0,0}, // 17
#endif
        
        {d_check_proc,CX+88,CY+4+96,64,16,0,-1,0,D_DISABLED,1,0,  "Sound",0,0}, // 18
        
        {d_text_proc,CX+8,CY+4+112,40,8,0,-1,0,0,0,0,"Refresh rate :",0,0}, // 19
        {d_slider_proc,CX+8,CY+4+120,104,16,0,-1,0,0,80/5,0,NULL,hzcallback,0}, // 20
        {d_text_proc,CX+112,CY+4+120+4+1,40,8,0,-1,0,0,0,0,NULL,0,0}, //21
        
        {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

static DIALOG rpcemugui[]=
{
        {d_menu_proc,  0,   0,   0,  0, 15,0,0,0,     0,0,mainmenu,NULL,NULL},
	{d_yield_proc,  0,   0,   0,  0, 15,0,0,0,     0,0,NULL,NULL,NULL},
      {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

void entergui()
{
        DIALOG_PLAYER *dp;
        int x = 1;
        infocus=0;
        
        if (soundenabled) stopsound();
        
        settingsmenu[1].flags=(fullscreen)?D_SELECTED:0;
        settingsmenu[2].flags=(stretchmode)?D_SELECTED:0;
        settingsmenu[3].flags=(skipblits)?D_SELECTED:0;
        settingsmenu[4].flags=(mousehackon)?D_SELECTED:0;
        
        dp=init_dialog(rpcemugui,0);
        show_mouse(screen);
        while (x && !(mouse_b&2) && !key[KEY_ESC])
        {
                x=update_dialog(dp);
        }
        show_mouse(NULL);
        shutdown_dialog(dp);

        clear(screen);
        clear_keybuf();
        resetbuffer();
        
        if (soundenabled) continuesound();
        
        infocus=1;
        return;
}
