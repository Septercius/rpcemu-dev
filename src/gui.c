#include <stdint.h>
#include <allegro.h>
#include "rpcemu.h"
#include "vidc20.h"
#include "sound.h"
#include "iomd.h"
#include "fdc.h"
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

/* Indexes into the settingsmenu array */
//#define MENU_SETTINGS_SETTINGS_WINDOW 0
#define MENU_SETTINGS_FULLSCREEN      1
#define MENU_SETTINGS_ALT_BLIT        2
#define MENU_SETTINGS_BLIT_OPTIMISE   3
#define MENU_SETTINGS_MOUSEHACK       4
//#define MENU_SETTINGS_CDROM_SUBMENU   5

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
	if (config.cdromenabled)
	{
		res=alert("This will reset RPCemu!","Okay to continue?",NULL,"OK","Cancel",0,0);
		if (res==1)
		{
			config.cdromenabled = 0;
			resetrpc();
		}
	}
	return D_CLOSE;
}

static int cdempty(void)
{
	int res;
	if (!config.cdromenabled)
	{
		res=alert("This will reset RPCemu!","Okay to continue?",NULL,"OK","Cancel",0,0);
		if (res==1)
		{
			config.cdromenabled = 1;
			resetrpc();
		}
		else		   
	 	   return D_CLOSE;
	}
	atapi->exit();
	iso_init();
	return D_CLOSE;
}

static int cdisoimage(void)
{
        char fn[260];
        int ret,res;
        int xsize=SCREEN_W-32,ysize=SCREEN_H-64;
	if (!config.cdromenabled)
	{
		res=alert("This will reset RPCemu!","Okay to continue?",NULL,"OK","Cancel",0,0);
		if (res==1)
		{
			config.cdromenabled = 1;
			resetrpc();
		}
		else
	   	   return D_EXIT;
	}
	memcpy(fn, config.isoname, 260);
	ret=file_select_ex("Please choose a disc image",fn,"ISO",260,xsize,ysize);
	if (ret) {
		strcpy(config.isoname, fn);
		atapi->exit();
		iso_open(config.isoname);
	}
        return D_EXIT;
}

#if defined linux || defined __linux
static int cdioctl(void)
{
	int res;
	if (!config.cdromenabled)
	{
		res=alert("This will reset RPCemu!","Okay to continue?",NULL,"OK","Cancel",0,0);
		if (res==1)
		{
			config.cdromenabled = 1;
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
        settingsmenu[MENU_SETTINGS_FULLSCREEN].flags = fullscreen ? D_SELECTED : 0;
        return D_CLOSE;
}

static int menualt(void)
{
        config.stretchmode ^= 1;
        settingsmenu[MENU_SETTINGS_ALT_BLIT].flags = config.stretchmode ? D_SELECTED : 0;
        return D_CLOSE;
}

static int menublt(void)
{
        config.skipblits ^= 1;
        settingsmenu[MENU_SETTINGS_BLIT_OPTIMISE].flags = config.skipblits ? D_SELECTED : 0;
        return D_CLOSE;
}

static int menumouse(void)
{
        config.mousehackon ^= 1;
        settingsmenu[MENU_SETTINGS_MOUSEHACK].flags = config.mousehackon ? D_SELECTED : 0;
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

        switch (config.model) {
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
                fprintf(stderr, "Unknown CPU model %d\n", config.model);
                exit(EXIT_FAILURE);
        }

        configuregui[CONF_RAM_4].flags   = 0;
        configuregui[CONF_RAM_8].flags   = 0;
        configuregui[CONF_RAM_16].flags  = 0;
        configuregui[CONF_RAM_32].flags  = 0;
        configuregui[CONF_RAM_64].flags  = 0;
        configuregui[CONF_RAM_128].flags = 0;
        switch (config.rammask)
        {
                case 0x01FFFFF: configuregui[CONF_RAM_4].flags   = D_SELECTED; break;
                case 0x03FFFFF: configuregui[CONF_RAM_8].flags   = D_SELECTED; break;
                case 0x07FFFFF: configuregui[CONF_RAM_16].flags  = D_SELECTED; break;
                case 0x0FFFFFF: configuregui[CONF_RAM_32].flags  = D_SELECTED; break;
                case 0x1FFFFFF: configuregui[CONF_RAM_64].flags  = D_SELECTED; break;
                case 0x3FFFFFF: configuregui[CONF_RAM_128].flags = D_SELECTED; break;
        }

        configuregui[CONF_VRAM_0].flags = config.vrammask     ? 0          : D_SELECTED;
        configuregui[CONF_VRAM_2].flags = config.vrammask     ? D_SELECTED : 0;
        configuregui[CONF_SOUND].flags  = config.soundenabled ? D_SELECTED : 0;
        configuregui[CONF_HZ_SLIDER].d2 = (config.refresh - 20) / 5;
        sprintf(hzstring, "%iHz", config.refresh);
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

                if (config.model != selected_model) {
                        config.model = selected_model;
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
                if (config.rammask != selected_rammask) {
                        config.rammask = selected_rammask;
                        changed = 1;
                }

                if (configuregui[CONF_VRAM_0].flags & D_SELECTED) {
                        selected_vrammask = 0;
                } else if (configuregui[CONF_VRAM_2].flags & D_SELECTED) {
                        selected_vrammask = 0x7FFFFF;
                }
                if (config.vrammask != selected_vrammask) {
                        config.vrammask = selected_vrammask;
                        changed  = 1;
                }

                if (changed)
                        resetrpc();
                
                config.refresh = (configuregui[CONF_HZ_SLIDER].d2 * 5) + 20;
                
                if (config.soundenabled && !(configuregui[CONF_SOUND].flags & D_SELECTED)) {
                        config.soundenabled = 0;
                        sound_pause();
                }
                if (!config.soundenabled && (configuregui[CONF_SOUND].flags & D_SELECTED)) {
                        config.soundenabled = 1;
                        sound_restart();
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
        {d_radio_proc,CX+8,CY+4,64,16,0,-1,0,0, 0 ,0,"ARM7500",0,0},   // 4
        {d_radio_proc,CX+8,CY+4+16,64,16,0,-1,0,0, 0 ,0,"ARM610",0,0}, // 5
        {d_radio_proc,CX+8,CY+4+32,64,16,0,-1,0,0, 0 ,0,"ARM710",0,0}, // 6
        {d_radio_proc,CX+8,CY+4+48,64,16,0,-1,0,0, 0 ,0,"SA110",0,0},  // 7

        {d_text_proc,CX+88,CY-4,40,8,0,-1,0,0,0,0,"RAM :",0,0}, // 8
        {d_radio_proc,CX+88,CY+4,64,16,0,-1,0,0, 1, 0,"4mb",0,0}, // 9
        {d_radio_proc,CX+88,CY+4+16,64,16,0,-1,0,0, 1, 0,"8mb",0,0}, // 10
        {d_radio_proc,CX+88,CY+4+32,64,16,0,-1,0,0, 1, 0,"16mb",0,0}, // 11
        {d_radio_proc,CX+88,CY+4+48,64,16,0,-1,0,0, 1, 0,"32mb",0,0}, // 12
        {d_radio_proc,CX+88,CY+4+64,64,16,0,-1,0,0, 1, 0,"64mb",0,0}, // 13
        {d_radio_proc,CX+88,CY+4+80,64,16,0,-1,0,0, 1, 0,"128mb",0,0}, // 14
        
        {d_text_proc,CX+8,CY+4+72,40,8,0,-1,0,0,0,0,"VRAM :",0,0}, // 15
        {d_radio_proc,CX+8,CY+4+80,64,16,0,-1,0,0, 2, 0,"None",0,0}, // 16
        {d_radio_proc,CX+8,CY+4+96,64,16,0,-1,0,0, 2, 0,"2mb",0,0}, // 17
        
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

void entergui(void)
{
        DIALOG_PLAYER *dp;
        int x = 1;
        infocus=0;
        
        /* Prevent the contents of the sound buffer being repeatedly played
           whilst the user is configuring things */
        if (config.soundenabled) {
                sound_mute();
        }
        
        settingsmenu[MENU_SETTINGS_FULLSCREEN].flags    = fullscreen  ? D_SELECTED : 0;
        settingsmenu[MENU_SETTINGS_ALT_BLIT].flags      = config.stretchmode ? D_SELECTED : 0;
        settingsmenu[MENU_SETTINGS_BLIT_OPTIMISE].flags = config.skipblits   ? D_SELECTED : 0;
        settingsmenu[MENU_SETTINGS_MOUSEHACK].flags     = config.mousehackon ? D_SELECTED : 0;
        
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
        
        if (config.soundenabled) {
                sound_unmute();
        }
        
        infocus=1;
        return;
}
