#include <stdint.h>
#include <allegro.h>

#include "config.h"
#include "rpcemu.h"
#include "vidc20.h"
#include "sound.h"
#include "iomd.h"
#include "fdc.h"
#include "ide.h"
#include "cdrom-iso.h"
#include "network.h"

extern void ioctl_init(void);

/* Indexes into the configuregui array */
//#define CONF_BOX                   0
#define CONF_OK                    1
//#define CONF_CANCEL                2
//#define CONF_LABEL_CPU             3
#define CONF_ARM610                4
#define CONF_ARM710                5
#define CONF_SA110                 6
#define CONF_ARM7500               7
#define CONF_ARM7500FE             8
#define CONF_ARM810                9
//#define CONF_LABEL_RAM             10
#define CONF_RAM_4                11
#define CONF_RAM_8                12
#define CONF_RAM_16               13
#define CONF_RAM_32               14
#define CONF_RAM_64               15
#define CONF_RAM_128              16
//#define CONF_LABEL_VRAM           17
#define CONF_VRAM_0               18
#define CONF_VRAM_2               19
#define CONF_SOUND                20
//#define CONF_LABEL_HZ             21
#define CONF_HZ_SLIDER            22
#define CONF_HZ_TEXT              23

/* Indexes into the networkgui array */
//#define NETWORK_BOX                 0
#define NETWORK_OK                  1
//#define NETWORK_CANCEL              2
#define NETWORK_OFF                 3
#define NETWORK_ETHERNET_BRIDGING   4
//#define NETWORK_BRIDGENAME_LABEL    5
//#define NETWORK_BRIDGENAME_TEXT     6
#define NETWORK_IP_TUNNELLING       7
//#define NETWORK_IP_ADDRESS_LABEL    8
//#define NETWORK_IP_ADDRESS_TEXT     9

/* Indexes into the settingsmenu array */
//#define MENU_SETTINGS_SETTINGS_WINDOW 0
//#define MENU_SETTINGS_NETWORK_WINDOW 1
#define MENU_SETTINGS_FULLSCREEN      2
#define MENU_SETTINGS_ALT_BLIT        3
#define MENU_SETTINGS_BLIT_OPTIMISE   4
#define MENU_SETTINGS_MOUSEHACK       5
//#define MENU_SETTINGS_CDROM_SUBMENU   6

/* maximum number of bytes a single (UTF-8 encoded) character can have */
#define MAX_BYTES_PER_CHAR 4

#define IPADDRLEN 15  /* (123.123.123.123) */
#define BRNAMELEN 15
static char gui_ipaddress[(IPADDRLEN + 1) * MAX_BYTES_PER_CHAR] = "123.124.125.126";
static char gui_bridgename[(BRNAMELEN + 1) * MAX_BYTES_PER_CHAR] = "br0";

static DIALOG configuregui[];
static DIALOG networkgui[];

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
	if (ret) {
		rpcemu_floppy_load(0, fn);
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
	if (ret) {
		rpcemu_floppy_load(1, fn);
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
	{ "&Host CD/DVD Drive", cdioctl, NULL, 0, NULL },
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

/**
 * Function to prepare, display and handle user changes on the 'settings'
 * window.
 */
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
        case CPUModel_ARM7500FE:
                configuregui[CONF_ARM7500FE].flags = D_SELECTED;
                break;
        case CPUModel_ARM610:
                configuregui[CONF_ARM610].flags = D_SELECTED;
                break;
        case CPUModel_ARM710:
                configuregui[CONF_ARM710].flags = D_SELECTED;
                break;
        case CPUModel_ARM810:
                configuregui[CONF_ARM810].flags = D_SELECTED;
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
        
        if (c == CONF_OK) {
                CPUModel selected_model = CPUModel_ARM7500;
                int selected_rammask = 0;
                int selected_vrammask = 0;

                if (configuregui[CONF_ARM7500].flags & D_SELECTED) {
                        selected_model = CPUModel_ARM7500;
                } else if (configuregui[CONF_ARM7500FE].flags & D_SELECTED) {
                        selected_model = CPUModel_ARM7500FE;
                } else if (configuregui[CONF_ARM610].flags & D_SELECTED) {
                        selected_model = CPUModel_ARM610;
                } else if (configuregui[CONF_ARM710].flags & D_SELECTED) {
                        selected_model = CPUModel_ARM710;
                } else if (configuregui[CONF_ARM810].flags & D_SELECTED) {
                        selected_model = CPUModel_ARM810;
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

                /* If an A7000 (ARM7500) or an A7000+ (ARM7500FE) it does not have vram */
                if (config.model == CPUModel_ARM7500 || config.model == CPUModel_ARM7500FE) {
                        selected_vrammask = 0;
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

/**
 * Function to prepare, display and handle user changes on the 'networking'
 * window.
 */
static int menunetworking(void)
{
	int c;

	/* Prepare the GUI dialog, based on current dynamic settings */
	networkgui[NETWORK_OFF].flags = 0;
	networkgui[NETWORK_ETHERNET_BRIDGING].flags = 0;
	networkgui[NETWORK_IP_TUNNELLING].flags = 0;

	switch (config.network_type) {
	case NetworkType_Off:
		networkgui[NETWORK_OFF].flags = D_SELECTED;
		break;
	case NetworkType_EthernetBridging:
		networkgui[NETWORK_ETHERNET_BRIDGING].flags = D_SELECTED;
		break;
	case NetworkType_IPTunnelling:
		networkgui[NETWORK_IP_TUNNELLING].flags = D_SELECTED;
		break;
	default:
		rpclog("Unknown Network type model %d, defaulting to Off\n",
		       config.network_type);
		networkgui[NETWORK_OFF].flags = D_SELECTED;
	}

	if (config.ipaddress) {
		strncpy(gui_ipaddress, config.ipaddress, IPADDRLEN * MAX_BYTES_PER_CHAR);
	}

	if (config.bridgename) {
		strncpy(gui_bridgename, config.bridgename, BRNAMELEN * MAX_BYTES_PER_CHAR);
	}

	/* Display the GUI */
	centre_dialog(networkgui);
	c = popup_dialog(networkgui, 1);

	/* Handle the user clicking OK and anything changing */
	if (c == NETWORK_OK) {
		NetworkType selected_network_type = NetworkType_Off;

		if (networkgui[NETWORK_OFF].flags & D_SELECTED) {
			selected_network_type = NetworkType_Off;
		} else if (networkgui[NETWORK_ETHERNET_BRIDGING].flags & D_SELECTED) {
			selected_network_type = NetworkType_EthernetBridging;
		} else if (networkgui[NETWORK_IP_TUNNELLING].flags & D_SELECTED) {
			selected_network_type = NetworkType_IPTunnelling;
		} else {
			rpclog("Failed to extract a value for network_type from the GUI, defaulting to Off");
		}

		/* Pass on the values to the core, to see if we need to restart */
		if (network_config_changed(selected_network_type, gui_bridgename, gui_ipaddress)) {
			resetrpc();
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
#ifdef RPCEMU_NETWORKING
        { "&Networking...", menunetworking, NULL, 0, NULL },
#endif
        {"&Fullscreen mode",menufullscreen,NULL,0,NULL},
        {"&Alternative blitting code",menualt,NULL,0,NULL},
        {"&Blitting optimisation",menublt,NULL,0,NULL},
        { "Follow host &mouse", menumouse, NULL, 0, NULL },
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
        {d_shadow_box_proc, CX,CY-8, 168,208,0,-1,0,0,     0,0,0,0,0}, // 0
        
        {d_button_proc,CX+8, CY+176,64, 16, 0,-1,0,D_EXIT,0,0,"OK",0,0}, // 1
        {d_button_proc,CX+96,CY+176,64, 16, 0,-1,0,D_EXIT,0,0,"Cancel",0,0}, // 2

        {d_text_proc,CX+8,CY-4,40,8,0,-1,0,0,0,0,"CPU :",0,0}, // 3
        {d_radio_proc,CX+8,CY+4,   64,16,0,-1,0,0, 0 ,0,"ARM610",0,0},    // 4
        {d_radio_proc,CX+8,CY+4+16,64,16,0,-1,0,0, 0 ,0,"ARM710",0,0},    // 5
        {d_radio_proc,CX+8,CY+4+32,64,16,0,-1,0,0, 0 ,0,"SA110",0,0},     // 6
        {d_radio_proc,CX+8,CY+4+48,64,16,0,-1,0,0, 0 ,0,"ARM7500",0,0},   // 7
        {d_radio_proc,CX+8,CY+4+64,64,16,0,-1,0,0, 0 ,0,"ARM7500FE",0,0}, // 8
        {d_radio_proc,CX+8,CY+4+80,64,16,0,-1,0,0, 0 ,0,"ARM810",0,0},    // 9


        {d_text_proc,CX+96,CY-4,40,8,0,-1,0,0,0,0,"RAM :",0,0}, // 10
        {d_radio_proc,CX+96,CY+4,64,16,0,-1,0,0, 1, 0,"4mb",0,0}, // 11
        {d_radio_proc,CX+96,CY+4+16,64,16,0,-1,0,0, 1, 0,"8mb",0,0}, // 12
        {d_radio_proc,CX+96,CY+4+32,64,16,0,-1,0,0, 1, 0,"16mb",0,0}, // 13
        {d_radio_proc,CX+96,CY+4+48,64,16,0,-1,0,0, 1, 0,"32mb",0,0}, // 14
        {d_radio_proc,CX+96,CY+4+64,64,16,0,-1,0,0, 1, 0,"64mb",0,0}, // 15
        {d_radio_proc,CX+96,CY+4+80,64,16,0,-1,0,0, 1, 0,"128mb",0,0}, // 16
        
        {d_text_proc,CX+8,CY+4+104,40,8,0,-1,0,0,0,0,"VRAM :",0,0}, // 17
        {d_radio_proc,CX+8,CY+4+112,64,16,0,-1,0,0, 2, 0,"None",0,0}, // 18
        {d_radio_proc,CX+8,CY+4+128,64,16,0,-1,0,0, 2, 0,"2mb",0,0}, // 19
        
        {d_check_proc,CX+96,CY+4+128,64,16,0,-1,0,D_DISABLED,1,0,  "Sound",0,0}, // 10
        
        {d_text_proc,CX+8,CY+4+144,40,8,0,-1,0,0,0,0,"Refresh rate :",0,0}, // 21
        {d_slider_proc,CX+8,CY+4+152,104,16,0,-1,0,0,80/5,0,NULL,hzcallback,0}, // 22
        {d_text_proc,CX+120,CY+4+152+4+1,40,8,0,-1,0,0,0,0,NULL,0,0}, //23
        
        {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

static DIALOG networkgui[] =
{
	/* proc, x, y, w, h, fg, bg */
	{ d_shadow_box_proc, CX, CY-8,           296,128, 0, -1,0,0,     0,0,0,0,0}, // 0

	{ d_button_proc,   CX+72, CY+96,          64, 16, 0, -1,0,D_EXIT,0,0,"OK",0,0}, // 1
	{ d_button_proc,   CX+160, CY+96,         64, 16, 0, -1,0,D_EXIT,0,0,"Cancel",0,0}, // 2

	{ d_radio_proc,    CX+8, CY+4,         24+16, 16, 0, -1,0,0,     2, 0, "Off",0,0}, // 18
	{ d_radio_proc,    CX+8, CY+4+16,     136+16, 16, 0, -1,0,0,     2, 0, "Ethernet Bridging",0,0}, // 18
	{ d_text_proc,     CX+8+16, CY+4+36,      40,  8, 0, -1,0,0,     0, 0, "Bridge Name :",0,0}, // 21
	{ d_edit_proc,     CX+8+16+104, CY+4+36, 160, 16, 0, 0xdddddd,0,0, BRNAMELEN, 0, gui_bridgename, NULL, NULL },
	{ d_radio_proc,    CX+8, CY+4+48,     104+16, 16, 0, -1,0,0,     2, 0, "IP Tunnelling",0,0}, // 18
	{ d_text_proc,     CX+8+16, CY+4+68,      40,  8, 0, -1,0,0,     0, 0, "IP Address  :",0,0}, // 21
	{ d_edit_proc,     CX+8+16+104, CY+4+68, 160, 16, 0, 0xdddddd,0,0, IPADDRLEN, 0, gui_ipaddress,  NULL, NULL },

	{ 0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL }
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
