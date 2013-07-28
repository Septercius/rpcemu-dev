#include <stdint.h>
#include <allegro.h>

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
#define CONF_BOX		 0
#define CONF_LABEL_CPU		 1
#define CONF_MODEL_LIST		 2
#define CONF_LABEL_RAM		 3
#define CONF_RAM_4		 4
#define CONF_RAM_8		 5
#define CONF_RAM_16		 6
#define CONF_RAM_32		 7
#define CONF_RAM_64		 8
#define CONF_RAM_128		 9
#define CONF_RAM_256		10
#define CONF_LABEL_VRAM		11
#define CONF_VRAM_0		12
#define CONF_VRAM_2		13
#define CONF_SOUND		14
#define CONF_LABEL_REFRESH	15
#define CONF_HZ_SLIDER		16
#define CONF_HZ_TEXT		17
#define CONF_OK			18
#define CONF_CANCEL		19

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
#ifdef RPCEMU_NETWORKING
 #define MENU_SETTINGS_SETTINGS_WINDOW 0
 #define MENU_SETTINGS_NETWORK_WINDOW  1
 #define MENU_SETTINGS_SEPARATOR_0     2
 #define MENU_SETTINGS_FULLSCREEN      3
 #define MENU_SETTINGS_CPU_IDLE        4
 #define MENU_SETTINGS_SEPARATOR_1     5
 #define MENU_SETTINGS_MOUSEHACK       6
 #define MENU_SETTINGS_MOUSETWOBUTTON  7
 #define MENU_SETTINGS_SEPARATOR_2     8
 #define MENU_SETTINGS_CDROM_SUBMENU   9
#else
 #define MENU_SETTINGS_SETTINGS_WINDOW 0
 #define MENU_SETTINGS_SEPARATOR_0     1
 #define MENU_SETTINGS_FULLSCREEN      2
 #define MENU_SETTINGS_CPU_IDLE        3
 #define MENU_SETTINGS_SEPARATOR_1     4
 #define MENU_SETTINGS_MOUSEHACK       5
 #define MENU_SETTINGS_MOUSETWOBUTTON  6
 #define MENU_SETTINGS_SEPARATOR_2     7
 #define MENU_SETTINGS_CDROM_SUBMENU   8
#endif

/* maximum number of bytes a single (UTF-8 encoded) character can have */
#define MAX_BYTES_PER_CHAR 4

static DIALOG configuregui[];

#ifdef RPCEMU_NETWORKING
#define IPADDRLEN 15  /* (123.123.123.123) */
#define BRNAMELEN 15

static DIALOG networkgui[];
static char gui_ipaddress[(IPADDRLEN + 1) * MAX_BYTES_PER_CHAR] = "123.124.125.126";
static char gui_bridgename[(BRNAMELEN + 1) * MAX_BYTES_PER_CHAR] = "br0";
#endif /* RPCEMU_NETWORKING */

/**
 * Callback function for File->Exit menu item.
 *
 * @return D_CLOSE - Close the dialog
 */
static int
menuexit(void)
{
        quited=1;
        return D_CLOSE;
}

/**
 * Callback function for File->Reset menu item.
 *
 * @return D_CLOSE - Close the dialog
 */
static int
menureset(void)
{
        resetrpc();
        return D_CLOSE;
}

static MENU filemenu[]=
{
	{ "&Reset", menureset, NULL, 0, NULL },
	{ "",       NULL,      NULL, 0, NULL },
	{ "E&xit",  menuexit,  NULL, 0, NULL },
	{ NULL,     NULL,      NULL, 0, NULL }
};

/**
 * Callback function when someone clicks OK on the file selection box
 * when loading disc 0
 *
 * @return D_EXIT - Close the dialog
 */
static int
menuld0(void)
{
        char fn[260];
        int ret;//,c;
        int xsize=SCREEN_W-32,ysize=SCREEN_H-64;

        /* Create file selection dialog to pick disk image */
        memcpy(fn,discname[0],260);
        ret=file_select_ex("Please choose a disc image",fn,"ADF",260,xsize,ysize);
	if (ret) {
		rpcemu_floppy_load(0, fn);
	}
        return D_EXIT;
}

/**
 * Callback function when someone clicks OK on the file selection box
 * when loading disc 1
 *
 * @return D_EXIT - Close the dialog
 */
static int
menuld1(void)
{
        char fn[260];
        int ret;//,c;
        int xsize=SCREEN_W-32,ysize=SCREEN_H-64;

        /* Create file selection dialog to pick disk image */
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

/**
 * Callback function for CD-ROM "Disabled" menu item.
 *
 * @return D_CLOSE - Close the dialog
 */
static int
cddisabled(void)
{
	int res;
	if (config.cdromenabled)
	{
		res = alert("This will reset RPCEmu!", "Okay to continue?", NULL, "OK", "Cancel", 0, 0);
		if (res==1)
		{
			config.cdromenabled = 0;
			resetrpc();
		}
	}
	return D_CLOSE;
}

/**
 *
 *
 * @return
 */
static int
cdempty(void)
{
	int res;
	if (!config.cdromenabled)
	{
		res = alert("This will reset RPCEmu!", "Okay to continue?", NULL, "OK", "Cancel", 0, 0);
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

/**
 * Callback function for CD-ROM "ISO Image ..." menu item.
 *
 * @return D_EXIT - Close the dialog
 */
static int
cdisoimage(void)
{
        char fn[260];
        int ret,res;
        int xsize=SCREEN_W-32,ysize=SCREEN_H-64;
	if (!config.cdromenabled)
	{
		res = alert("This will reset RPCEmu!", "Okay to continue?", NULL, "OK", "Cancel", 0, 0);
		if (res==1)
		{
			config.cdromenabled = 1;
			resetrpc();
		}
		else
	   	   return D_EXIT;
	}
	/* Create file selection dialog to pick iso image */
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
/**
 * Callback function for CD-ROM "Host CD/DVD drive" menu item.
 *
 * @return D_CLOSE - Close the dialog
 */
static int
cdioctl(void)
{
	int res;
	if (!config.cdromenabled)
	{
		res = alert("This will reset RPCEmu!", "Okay to continue?", NULL, "OK", "Cancel", 0, 0);
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

/**
 * Callback function for "Fullscreen mode" menu item.
 *
 * @return D_CLOSE - Close the dialog
 */
static int
menufullscreen(void)
{
        togglefullscreen(!fullscreen);
        settingsmenu[MENU_SETTINGS_FULLSCREEN].flags = fullscreen ? D_SELECTED : 0;
        return D_CLOSE;
}

/**
 * Callback function for "Reduce CPU usage" menu item.
 *
 * @return D_CLOSE - Close the dialog
 */
static int
menu_cpu_idle(void)
{
	int res;

	res = alert("This will reset RPCEmu!", "Okay to continue?", NULL, "OK", "Cancel", 0, 0);
	if (res == 1) {
		config.cpu_idle ^= 1;
		settingsmenu[MENU_SETTINGS_CPU_IDLE].flags = config.cpu_idle ? D_SELECTED : 0;
		resetrpc();
	}
	return D_CLOSE;
}

/**
 * Callback function for the "Follow host mouse" menu item.
 *
 * @return D_CLOSE - Close the dialog
 */
static int
menumouse(void)
{
        config.mousehackon ^= 1;
        settingsmenu[MENU_SETTINGS_MOUSEHACK].flags = config.mousehackon ? D_SELECTED : 0;
        return D_CLOSE;
}

/**
 * Callback function for the "Two-button Mouse Mode" menu item.
 *
 * @return D_CLOSE - Close the dialog
 */
static int
menutwobutton(void)
{
	config.mousetwobutton ^= 1;
	settingsmenu[MENU_SETTINGS_MOUSETWOBUTTON].flags =
	    config.mousetwobutton ? D_SELECTED : 0;
	return D_CLOSE;
}

static char hzstring[20];

/**
 * Function to prepare, display and handle user changes on the 'settings'
 * window.
 *
 * @return D_CLOSE Close the dialog
 */
static int
menusettings(void)
{
        int c;
        int changed=0;

	/* Select the currently chosen model */
	configuregui[CONF_MODEL_LIST].d1 = machine.model;

        configuregui[CONF_RAM_4].flags   = 0;
        configuregui[CONF_RAM_8].flags   = 0;
        configuregui[CONF_RAM_16].flags  = 0;
        configuregui[CONF_RAM_32].flags  = 0;
        configuregui[CONF_RAM_64].flags  = 0;
        configuregui[CONF_RAM_128].flags = 0;
        configuregui[CONF_RAM_256].flags = 0;
	switch (config.mem_size) {
	case 4:   configuregui[CONF_RAM_4].flags   = D_SELECTED; break;
	case 8:   configuregui[CONF_RAM_8].flags   = D_SELECTED; break;
	case 16:  configuregui[CONF_RAM_16].flags  = D_SELECTED; break;
	case 32:  configuregui[CONF_RAM_32].flags  = D_SELECTED; break;
	case 64:  configuregui[CONF_RAM_64].flags  = D_SELECTED; break;
	case 128: configuregui[CONF_RAM_128].flags = D_SELECTED; break;
	case 256: configuregui[CONF_RAM_256].flags = D_SELECTED; break;
	}

        configuregui[CONF_VRAM_0].flags = config.vrammask     ? 0          : D_SELECTED;
        configuregui[CONF_VRAM_2].flags = config.vrammask     ? D_SELECTED : 0;
        configuregui[CONF_SOUND].flags  = config.soundenabled ? D_SELECTED : 0;
        configuregui[CONF_HZ_SLIDER].d2 = (config.refresh - 20) / 5;
        sprintf(hzstring, "%iHz", config.refresh);
        configuregui[CONF_HZ_TEXT].dp = hzstring;

	centre_dialog(configuregui);

        c=popup_dialog(configuregui,1);

        if (c == CONF_OK) {
		Model selected_model;
                unsigned selected_mem_size = 0;
                int selected_vrammask = 0;

		selected_model = configuregui[CONF_MODEL_LIST].d1;
		if (selected_model != machine.model) {
			rpcemu_model_changed(selected_model);
			changed = 1;
		}

                if (configuregui[CONF_RAM_4].flags & D_SELECTED) {
                        selected_mem_size = 4;
                } else if (configuregui[CONF_RAM_8].flags & D_SELECTED) {
                        selected_mem_size = 8;
                } else if (configuregui[CONF_RAM_16].flags & D_SELECTED) {
                        selected_mem_size = 16;
                } else if (configuregui[CONF_RAM_32].flags & D_SELECTED) {
                        selected_mem_size = 32;
                } else if (configuregui[CONF_RAM_64].flags & D_SELECTED) {
                        selected_mem_size = 64;
                } else if (configuregui[CONF_RAM_128].flags & D_SELECTED) {
                        selected_mem_size = 128;
                } else if (configuregui[CONF_RAM_256].flags & D_SELECTED) {
                        selected_mem_size = 256;
                }
                if (config.mem_size != selected_mem_size) {
                        config.mem_size = selected_mem_size;
                        changed = 1;
                }

                if (configuregui[CONF_VRAM_0].flags & D_SELECTED) {
                        selected_vrammask = 0;
                } else if (configuregui[CONF_VRAM_2].flags & D_SELECTED) {
                        selected_vrammask = 0x7FFFFF;
                }

                /* If an A7000 or an A7000+ it does not have vram */
                if (machine.model == Model_A7000 || machine.model == Model_A7000plus) {
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

#ifdef RPCEMU_NETWORKING
/**
 * Function to prepare, display and handle user changes on the 'networking'
 * window.
 *
 * @return D_CLOSE Close the dialog
 */
static int
menunetworking(void)
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
#endif /* RPCEMU_NETWORKING */

/**
 * Callback function for the Refresh Rate slider
 *
 * @param dp3  not used
 * @param d2   value between 0 and 80/5 (20), 20 increments of 5Hz
 * @return D_CLOSE Close the dialog
 */
static int
hzcallback(void *dp3, int d2)
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

        return D_CLOSE;
}

static MENU settingsmenu[]=
{
	{ "&Settings...",           menusettings,   NULL,   0, NULL },
#ifdef RPCEMU_NETWORKING
	{ "&Networking...",         menunetworking, NULL,   0, NULL },
#endif /* RPCEMU_NETWORKING */
	{ "",                       NULL,           NULL,   0, NULL },
	{ "&Fullscreen mode",       menufullscreen, NULL,   0, NULL },
	{ "&Reduce CPU usage",      menu_cpu_idle,  NULL,   0, NULL },
	{ "",                       NULL,           NULL,   0, NULL },
	{ "Follow host &mouse",     menumouse,      NULL,   0, NULL },
	{ "&Two-button Mouse Mode", menutwobutton,  NULL,   0, NULL },
	{ "",                       NULL,           NULL,   0, NULL },
	{ "&CD-ROM",                NULL,           cdmenu, 0, NULL },
	{ NULL,                     NULL,           NULL,   0, NULL }
};

static MENU mainmenu[]=
{
        {"&File",NULL,filemenu,0,NULL},
        {"&Disc",NULL,discmenu,0,NULL},
        {"&Settings",NULL,settingsmenu,0,NULL},
        {NULL,NULL,NULL,0,NULL}
};

/**
 * Allegro callback to get the text of an entry in the models listbox, or to
 * get the number of entries on the list.
 *
 * @param index Index of entry to get (0 to sizeof(list -1) or -1 to get the size
 * @param list_size Pointer to an int to be filled in with list size, if index == -1
 * @return string of entry, or NULL if -1 (list size) was queried
 */
static const char *
model_listbox(int index, int *list_size)
{
	if (index < 0) {
		*list_size = Model_MAX;
		return NULL;
	} else {
		return models[index].name_gui;
	}
}

#define CY 0
#define CX 0
/* This array must be kept in sync with the CONF_ defines above */
static DIALOG configuregui[] = {
	{ d_shadow_box_proc, 0, 0, 37*8, 35*8, 0,-1,0,0, 0,0,0,0,0 }, // 0

	{ d_text_proc,   2*8, 2*8, 84, 16, 0,-1,0,0, 0, 0,"Hardware:",0,0 }, // 1
	{ d_list_proc,   3*8, 4*8, 22*8, 12*8, 0,0xffffff,0,0, 0, 0, model_listbox, NULL, NULL },  // 2

	{ d_text_proc,  27*8,  2*8, 40, 16, 0,-1,0,0, 0, 0,"RAM:",0,0 }, // 3
	{ d_radio_proc, 28*8,  3*8, 36, 16, 0,-1,0,0, 1, 0,"4MB",0,0 }, // 4
	{ d_radio_proc, 28*8,  5*8, 36, 16, 0,-1,0,0, 1, 0,"8MB",0,0 }, // 5
	{ d_radio_proc, 28*8,  7*8, 44, 16, 0,-1,0,0, 1, 0,"16MB",0,0 }, // 6
	{ d_radio_proc, 28*8,  9*8, 44, 16, 0,-1,0,0, 1, 0,"32MB",0,0 }, // 7
	{ d_radio_proc, 28*8, 11*8, 44, 16, 0,-1,0,0, 1, 0,"64MB",0,0 }, // 8
	{ d_radio_proc, 28*8, 13*8, 52, 16, 0,-1,0,0, 1, 0,"128MB",0,0 }, // 9
	{ d_radio_proc, 28*8, 15*8, 52, 16, 0,-1,0,0, 1, 0,"256MB",0,0 }, // 10

	{ d_text_proc,   2*8, 18*8, 52, 16, 0,-1,0,0, 0, 0,"VRAM:",0,0 }, // 11
	{ d_radio_proc,  3*8, 19*8, 44, 16, 0,-1,0,0, 2, 0,"None",0,0 }, // 12
	{ d_radio_proc,  3*8, 21*8, 36, 16, 0,-1,0,0, 2, 0,"2MB",0,0 }, // 13

	{ d_check_proc, 26*8, 19*8, 52, 16, 0,-1,0,D_DISABLED,1,0, "Sound",0,0 }, // 14

	{ d_text_proc,    2*8, 25*8,   40,  8, 0,-1,0,0,0,0,"Video refresh rate:",0,0 }, // 15
	{ d_slider_proc,  3*8, 27*8,  192, 16, 0,-1,0,0,80/5,0,NULL,hzcallback,0 }, // 16
	{ d_text_proc,   28*8, 27*8+5, 40,  8, 0,-1,0,0,0,0,NULL,0,0 }, // 17

	{ d_button_proc, 10*8, 31*8, 64, 16, 0,-1,0,D_EXIT,0,0,"OK",0,0 }, // 18
	{ d_button_proc, 20*8, 31*8, 64, 16, 0,-1,0,D_EXIT,0,0,"Cancel",0,0 }, // 19

	{ NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL }
};

#ifdef RPCEMU_NETWORKING
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
#endif /* RPCEMU_NETWORKING */

static DIALOG rpcemugui[]=
{
        {d_menu_proc,  0,   0,   0,  0, 15,0,0,0,     0,0,mainmenu,NULL,NULL},
	{d_yield_proc,  0,   0,   0,  0, 15,0,0,0,     0,0,NULL,NULL,NULL},
      {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

/**
 *
 */
void
entergui(void)
{
        DIALOG_PLAYER *dp;
        int x = TRUE;
        infocus=0;
        
        /* Prevent the contents of the sound buffer being repeatedly played
           whilst the user is configuring things */
        if (config.soundenabled) {
                sound_mute();
        }

        /* Update the dynamic menu items based on their current settings */
        settingsmenu[MENU_SETTINGS_FULLSCREEN].flags    = fullscreen  ? D_SELECTED : 0;
        settingsmenu[MENU_SETTINGS_CPU_IDLE].flags      = config.cpu_idle ? D_SELECTED : 0;
        settingsmenu[MENU_SETTINGS_MOUSEHACK].flags     = config.mousehackon ? D_SELECTED : 0;
        settingsmenu[MENU_SETTINGS_MOUSETWOBUTTON].flags =
            config.mousetwobutton ? D_SELECTED : 0;

        /* Enable the Allegro popover gui */
        dp=init_dialog(rpcemugui,0);
        show_mouse(screen);
        while (x != FALSE && !(mouse_b & 2) && !key[KEY_ESC])
        {
                x=update_dialog(dp);
        }
        show_mouse(NULL);
        shutdown_dialog(dp);

        clear(screen);
        clear_keybuf();
        resetbuffer();
        
        /* Re-enable the sound system */
        if (config.soundenabled) {
                sound_unmute();
        }
        
        infocus=1;
        return;
}
