#include <stdint.h>
#include <allegro.h>
#include "rpcemu.h"
#include "vidc20.h"
#include "sound.h"
#include "iomd.h"
#include "82c711.h"

DIALOG configuregui[];
int menuexit(void)
{
        quited=1;
        return D_CLOSE;
}

int menureset(void)
{
        resetrpc();
        return D_CLOSE;
}

MENU filemenu[]=
{
        {"&Reset",menureset,NULL,0,NULL},
        {"E&xit",menuexit,NULL,0,NULL},
        {NULL,NULL,NULL,0,NULL}
};

int menuld0(void)
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

int menuld1(void)
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

MENU discmenu[]=
{
        {"Load drive :&0...",menuld0,NULL,0,NULL},
        {"Load drive :&1...",menuld1,NULL,0,NULL},
        {NULL,NULL,NULL,0,NULL}
};

MENU settingsmenu[];

int menualt(void)
{
        stretchmode^=1;
        settingsmenu[1].flags=(stretchmode)?D_SELECTED:0;
        return D_CLOSE;
}

int menublt(void)
{
        skipblits^=1;
        settingsmenu[2].flags=(skipblits)?D_SELECTED:0;
        return D_CLOSE;
}

int menumouse(void)
{
        mousehackon^=1;
        settingsmenu[3].flags=(mousehackon)?D_SELECTED:0;
        return D_CLOSE;
}

char hzstring[20];
int menusettings(void)
{
        int c,d;
        int changed=0;
        for (c=0;c<4;c++) configuregui[4+c].flags=0;
        configuregui[4+model].flags=D_SELECTED;
        for (c=0;c<6;c++) configuregui[9+c].flags=0;
        switch (rammask)
        {
                case 0x1FFFFF: configuregui[9].flags=D_SELECTED; break;
                case 0x3FFFFF: configuregui[10].flags=D_SELECTED; break;
                case 0x7FFFFF: configuregui[11].flags=D_SELECTED; break;
                case 0xFFFFFF: configuregui[12].flags=D_SELECTED; break;
                case 0x1FFFFFF: configuregui[13].flags=D_SELECTED; break;
                case 0x3FFFFFF: configuregui[14].flags=D_SELECTED; break;
        }
        configuregui[16].flags=(vrammask)?0:D_SELECTED;
        configuregui[17].flags=(vrammask)?D_SELECTED:0;
        configuregui[18].flags=(soundenabled)?D_SELECTED:0;
        configuregui[20].d2=(refresh-20)/5;
        sprintf(hzstring,"%ihz",refresh);
        configuregui[21].dp=hzstring;
        
        position_dialog(configuregui,(SCREEN_W/2)-80,(SCREEN_H/2)-88);
        
        c=popup_dialog(configuregui,1);

        position_dialog(configuregui,-((SCREEN_W/2)-80),-((SCREEN_H/2)-88));
        
        if (c==1)
        {
                d=0;
                for (c=0;c<4;c++) d=(configuregui[4+c].flags&D_SELECTED)?c:d;
                if (model!=d) { model=d; changed=1; }
                d=0;
                for (c=0;c<6;c++) d=(configuregui[9+c].flags&D_SELECTED)?c:d;
                d=(0x200000<<d)-1;
                if (rammask!=d) { rammask=d; changed=1; }
                d=(configuregui[17].flags&D_SELECTED)?0x1FFFFF:0;
                if (vrammask!=d) { vrammask=d; changed=1; }
                if (changed) resetrpc();
                
                refresh=(configuregui[20].d2*5)+20;
                
                if (soundenabled && !(configuregui[18].flags&D_SELECTED)) { closesound(); soundenabled=0; }
                if (!soundenabled && (configuregui[18].flags&D_SELECTED)) { initsound(); soundenabled=1; }
        }
        return D_CLOSE;
}

int hzcallback(void *dp3, int d2)
{
        sprintf(hzstring,"%ihz",(d2*5)+20);
        configuregui[21].dp=hzstring;
        rectfill(screen,configuregui[21].x,configuregui[21].y,configuregui[21].x+39,configuregui[21].y+7,0xFFFFFFFF);
        object_message(&configuregui[21], MSG_DRAW, 0);

        return 0;
}

MENU settingsmenu[]=
{
        {"&Settings...",menusettings,NULL,0,NULL},
        {"&Alternative blitting code",menualt,NULL,0,NULL},
        {"&Blitting optimisation",menublt,NULL,0,NULL},
        {"&Mouse hack",menumouse,NULL,0,NULL},
        {NULL,NULL,NULL,0,NULL}
};

MENU mainmenu[]=
{
        {"&File",NULL,filemenu,0,NULL},
        {"&Disc",NULL,discmenu,0,NULL},
        {"&Settings",NULL,settingsmenu,0,NULL},
        {NULL,NULL,NULL,0,NULL}
};

#define CY 0
#define CX 0
DIALOG configuregui[]=
{
        {d_shadow_box_proc, CX,CY-8, 160,176,0,0xFFFFFFFF,0,0,     0,0,0,0,0},
        
        {d_button_proc,CX+8,CY+144,64, 16, 0,0xFFFFFFFF,0,D_EXIT,0,0,"OK",0,0},
        {d_button_proc,CX+88,CY+144,64, 16, 0,0xFFFFFFFF,0,D_EXIT,0,0,"Cancel",0,0},
        
        {d_text_proc,CX+8,CY-4,40,8,0,0xFFFFFFFF,0,0,0,0,"CPU :",0,0},
        {d_radio_proc,CX+8,CY+4,64,16,0,0xFFFFFFFF,0,0, 0 ,0,"ARM7500",0,0},
        {d_radio_proc,CX+8,CY+4+16,64,16,0,0xFFFFFFFF,0,0, 0 ,0,"ARM610",0,0},
        {d_radio_proc,CX+8,CY+4+32,64,16,0,0xFFFFFFFF,0,0, 0 ,0,"ARM710",0,0},
        {d_radio_proc,CX+8,CY+4+48,64,16,0,0xFFFFFFFF,0,0, 0 ,0,"SA110",0,0},
        
        {d_text_proc,CX+88,CY-4,40,8,0,0xFFFFFFFF,0,0,0,0,"RAM :",0,0},
        {d_radio_proc,CX+88,CY+4,64,16,0,0xFFFFFFFF,0,0, 1, 0,"4mb",0,0},
        {d_radio_proc,CX+88,CY+4+16,64,16,0,0xFFFFFFFF,0,0, 1, 0,"8mb",0,0},
        {d_radio_proc,CX+88,CY+4+32,64,16,0,0xFFFFFFFF,0,0, 1, 0,"16mb",0,0},
        {d_radio_proc,CX+88,CY+4+48,64,16,0,0xFFFFFFFF,0,0, 1, 0,"32mb",0,0},
        {d_radio_proc,CX+88,CY+4+64,64,16,0,0xFFFFFFFF,0,0, 1, 0,"64mb",0,0},
        {d_radio_proc,CX+88,CY+4+80,64,16,0,0xFFFFFFFF,0,0, 1, 0,"128mb",0,0},
        
        {d_text_proc,CX+8,CY+4+72,40,8,0,0xFFFFFFFF,0,0,0,0,"VRAM :",0,0},
        {d_radio_proc,CX+8,CY+4+80,64,16,0,0xFFFFFFFF,0,0, 2, 0,"None",0,0},
        {d_radio_proc,CX+8,CY+4+96,64,16,0,0xFFFFFFFF,0,0, 2, 0,"2mb",0,0},
        
        {d_check_proc,CX+88,CY+4+96,64,16,0,0xFFFFFFFF,0,0,1,0,  "Sound",0,0},
        
        {d_text_proc,CX+8,CY+4+112,40,8,0,0xFFFFFFFF,0,0,0,0,"Refresh rate :",0,0},
        {d_slider_proc,CX+8,CY+4+120,104,16,0,0xFFFFFFFF,0,0,80/5,0,NULL,hzcallback,0},
        {d_text_proc,CX+112,CY+4+120+4+1,40,8,0,0xFFFFFFFF,0,0,0,0,NULL,0,0},
        
        {0,0,0,0,0,0,0,0,0,0,0,NULL,NULL,NULL}
};

DIALOG rpcemugui[]=
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
        
        settingsmenu[1].flags=(stretchmode)?D_SELECTED:0;
        settingsmenu[2].flags=(skipblits)?D_SELECTED:0;
        settingsmenu[3].flags=(mousehackon)?D_SELECTED:0;
        
        dp=init_dialog(rpcemugui,0);
        show_mouse(screen);
        while (x && !(mouse_b&2) && !key[KEY_ESC])
        {
                x=update_dialog(dp);
        }
        show_mouse(NULL);
        shutdown_dialog(dp);
        
        clear_keybuf();
        resetbuffer();
        
        if (soundenabled) continuesound();
        
        infocus=1;
        return;
}
