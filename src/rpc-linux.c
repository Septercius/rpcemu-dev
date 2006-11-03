/*RPCemu v0.3 by Tom Walker
  Main loop*/

#include <allegro.h>
#include "rpc.h"

#define MB_OK 1
static void MessageBox(void *param, const char *message, 
		       const char *title, int type) {
  printf("MessageBox: %s %s\n", title, message);
}



int mousecapture=0;
float mips;
int updatemips=0;

static uint32_t mipscount;
float mipstotal;

static void domips(void)
{
        mips=(float)inscount/1000000.0f;
	mipscount += 1;
	if (mipscount > 10)
	  mipstotal += mips;
        inscount=0;
        updatemips=1;
}

void error(const char *format, ...)
{
   char buf[256];

   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   MessageBox(NULL,buf,"RPCemu error",MB_OK);;
}

FILE *arclog;
void rpclog(const char *format, ...)
{
   char buf[256];
return;
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,arclog);
}

int drawscre=0,flyback;

void sndupdate(void)
{
        dumpsound();
        iomd.state|=0x10;
        updateirqs();
        iomd.sndstat ^= 1;
        iomd.sndstat |= 6;
}

void vblupdate(void)
{
        drawscre=1;
//        iomdvsync();
        flyback=20;
}
void vblupdate2(void)
{
//        drawscre=1;
//        flyback=0;
}

/*  Declare Windows procedure  */
//LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);
//RECT oldclip,arcclip;

/*  Make the class name into a global variable  */
char szClassName[ ] = "WindowsApp";
//typedef unsigned int HWND;
//HWND ghwnd;
//HMENU menu;
int infocus;

#define IDM_FILE_RESET    40000
#define IDM_FILE_EXIT     40001
#define IDM_DISC_LD0      40010
#define IDM_DISC_LD1      40011
#define IDM_MACHINE_CPU   40020
#define IDM_MACHINE_RAM   40021
#define IDM_CPU_ARM610    40030
#define IDM_CPU_ARM710    40031
#define IDM_CPU_ARM7500   40032
#define IDM_RAM_4MEGS     40040
#define IDM_RAM_8MEGS     40041
#define IDM_RAM_16MEGS    40042

void updatewindowsize(uint32_t x, uint32_t y)
{
  printf("updatewindowsize: %u %u\n", x, y);

  set_gfx_mode(GRAPHICS_TYPE, x, y, 0, 0);

#if 0
        RECT r;
        GetWindowRect(ghwnd,&r);
        MoveWindow(ghwnd,r.left,r.top,
                     x+(GetSystemMetrics(SM_CXFIXEDFRAME)*2)+2,
                     y+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+3,
                     TRUE);
        if (mousecapture)
        {
                ClipCursor(&oldclip);
                GetWindowRect(ghwnd,&arcclip);
                arcclip.left+=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                arcclip.right-=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                arcclip.top+=GetSystemMetrics(SM_CXFIXEDFRAME)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+10;
                arcclip.bottom-=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                ClipCursor(&arcclip);
        }
#endif
}

void makemenu()
{
  puts("makemenu");
#if 0
        HMENU hpop,hpop2;
        menu=CreateMenu();
        hpop=CreateMenu();
        AppendMenu(hpop,MF_STRING,IDM_FILE_RESET,"&Reset");
        AppendMenu(hpop,MF_SEPARATOR,0,NULL);
        AppendMenu(hpop,MF_STRING,IDM_FILE_EXIT,"E&xit");
        AppendMenu(menu,MF_POPUP,hpop,"&File");
        hpop=CreateMenu();
        AppendMenu(hpop,MF_STRING,IDM_DISC_LD0,"Load drive &0...");
        AppendMenu(hpop,MF_STRING,IDM_DISC_LD1,"Load drive &1...");
        AppendMenu(menu,MF_POPUP,hpop,"&Disc");
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_CPU_ARM610,"ARM&610");
        AppendMenu(hpop2,MF_STRING,IDM_CPU_ARM710,"ARM&710");
        AppendMenu(hpop2,MF_STRING,IDM_CPU_ARM7500,"ARM7&500");
        hpop=CreateMenu();
        AppendMenu(hpop,MF_POPUP,hpop2,"&CPU");
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_RAM_4MEGS,"&4 megabytes");
        AppendMenu(hpop2,MF_STRING,IDM_RAM_8MEGS,"&8 megabytes");
        AppendMenu(hpop2,MF_STRING,IDM_RAM_16MEGS,"&16 megabytes");
        AppendMenu(hpop,MF_POPUP,hpop2,"&RAM");
        AppendMenu(menu,MF_POPUP,hpop,"&Machine");
#endif
}

void releasemousecapture()
{
#if 0
        if (mousecapture)
        {
                ClipCursor(&oldclip);
                mousecapture=0;
        }
#endif
}

char discname[2][260]={"boot.adf","notboot.adf"};

void updatemenu()
{
#if 0
        CheckMenuItem(menu,IDM_CPU_ARM610,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_CPU_ARM710,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_CPU_ARM7500,MF_UNCHECKED);
        switch (model)
        {
                case 1: CheckMenuItem(menu,IDM_CPU_ARM610,MF_CHECKED);  break;
                case 2: CheckMenuItem(menu,IDM_CPU_ARM710,MF_CHECKED);  break;
                case 0: CheckMenuItem(menu,IDM_CPU_ARM7500,MF_CHECKED); break;
        }
        CheckMenuItem(menu,IDM_RAM_4MEGS,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_RAM_8MEGS,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_RAM_16MEGS,MF_UNCHECKED);
        switch (rammask)
        {
                case 0x3FFFFF: CheckMenuItem(menu,IDM_RAM_4MEGS,MF_CHECKED);  break;
                case 0x7FFFFF: CheckMenuItem(menu,IDM_RAM_8MEGS,MF_CHECKED);  break;
                case 0xFFFFFF: CheckMenuItem(menu,IDM_RAM_16MEGS,MF_CHECKED); break;
        }
#endif
}

void resetrpc()
{
        memset(ram,0,rammask+1);
        resetcp15();
        resetarm();
        resetkeyboard();
        resetiomd();
        reseti2c();
        resetide();
        reset82c711();
}

#if 0
int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)
#endif

int main (void) 

{
  //        MSG messages;            /* Here messages to the application are saved */
  //      WNDCLASSEX wincl;        /* Data structure for the windowclass */
        int quited=0;
        char s[128];
        const char *p;
	//      unsigned char d;
        //FILE *f;
        //int c;
        char fn[512];

#if 0
        /* The Window structure */
        wincl.hInstance = hThisInstance;
        wincl.lpszClassName = szClassName;
        wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
        wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
        wincl.cbSize = sizeof (WNDCLASSEX);

        /* Use default icon and mouse-pointer */
        wincl.hIcon = LoadIcon(hThisInstance, "allegro_icon");
        wincl.hIconSm = LoadIcon(hThisInstance, "allegro_icon");
        wincl.hCursor = LoadCursor (NULL, IDC_ARROW);
        wincl.lpszMenuName = NULL;                 /* No menu */
        wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
        wincl.cbWndExtra = 0;                      /* structure or the window instance */
        /* Use Windows's default color as the background of the window */
        wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

        /* Register the window class, and if it fails quit the program */
        if (!RegisterClassEx (&wincl))
           return 0;
#endif
        makemenu();
        /* The class is registered, let's create the program*/
#if 0
        ghwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           "RPCemu v0.3",       /* Title Text */
           WS_OVERLAPPEDWINDOW, /* default window */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           640+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),/* The programs width */
           480+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+2,/* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           menu,                /* No menu */
           hThisInstance,       /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

        /* Make the window visible on the screen */
        ShowWindow (ghwnd, nFunsterStil);
        win_set_window(ghwnd);
#endif
        allegro_init();
//        arclog=fopen("arclog.txt","wt");
        get_executable_name(exname,511);
        p=get_filename(exname);
        *(char *)p=0;
        append_filename(fn,exname,"rpc.cfg",511);
        set_config_file(fn);
        p=get_config_string(NULL,"mem_size",NULL);
        if (!p || !strcmp(p,"4")) rammask=0x3FFFFF;
        else if (!strcmp(p,"8"))  rammask=0x7FFFFF;
        else if (!strcmp(p,"16")) rammask=0x7FFFFF;
        else if (!strcmp(p,"32")) rammask=0xFFFFFF;
        else if (!strcmp(p,"64")) rammask=0x1FFFFFF;
        else                      rammask=0x3FFFFFF;

        p=get_config_string(NULL,"vram_size",NULL);
        if (!p || !strcmp(p,"0"))  vrammask=0;
        else                       vrammask=0x1FFFFF;

        p=get_config_string(NULL,"cpu_type",NULL);
        if (!p || !strcmp(p,"ARM610")) model=1;
        else if (!strcmp(p,"ARM710"))  model=2;
        else                           model=0;
        initmem();
        if (loadroms())
        {
                MessageBox(NULL,"RISC OS ROMs missing!","RISC OS ROMs missing!",MB_OK);
                return 0;
        }
        resetarm();
        resetiomd();
        resetkeyboard();
        reset82c711();
        resetide();
        reseti2c();
        loadcmos();
        loadadf("boot.adf",0);
        loadadf("notboot.adf",1);
        initvideo();
        install_keyboard();
        install_timer();
        atexit(releasemousecapture);
        install_mouse();
        install_int_ex(domips,MSEC_TO_TIMER(1000));
        install_int_ex(vblupdate,BPS_TO_TIMER(60));
        install_int_ex(sndupdate,BPS_TO_TIMER(100));
        reallocmem(rammask);
//        model=0;
        vrammask=0;//x1FFFFF;
//        rammask=0x3FFFFF;
        updatemenu();
//        rest(5);
//        install_int_ex(vblupdate2,BPS_TO_TIMER(60));
        while (!quited)
        {
                execarm(80000);
                if (drawscre)
                {
		        usleep(50);

                        drawscre=0;
                        iomdvsync();
//                        execarm(1000);
                        drawscr();
                        pollmouse();
                        pollkeyboard();
                        drawscre=0;
                        if (updatemips && mipscount > 10)
                        {                           
			  printf("MIPS: %f (AVG: %f)\n", mips, mipstotal / (mipscount - 10));
			        //sprintf(s,"RPCemu v0.3 - %f MIPS - %s",mips,(mousecapture)?"Press CTRL-END to release mouse":"Click to capture mouse");
				//                                SetWindowText(ghwnd, s);
                                updatemips=0;
                        }
                }
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && mousecapture)
                {
		  //                        ClipCursor(&oldclip);
                        mousecapture=0;
                        updatemips=1;
                }
/*This is a useful method of loading stuff into RAM to be saved to hard disc.
  It's not the most trivial of methods though, and can happen accidently, so I've
  commented it out for now*/
/*                if (key[KEY_HOME])
                {
                        f=fopen("bubimp.zip","rb");
                        c=0x8000;
                        while (!feof(f))
                        {
                                d=getc(f);
                                writememb(c,d);
                                c++;
                        }
                        fclose(f);
                }*/
#if 0
                if (PeekMessage(&messages,NULL,0,0,PM_REMOVE))
                {
                        if (messages.message==WM_QUIT)
                           quited=1;
                        /* Translate virtual-key messages into character messages */
                        TranslateMessage(&messages);
                        /* Send message to WindowProcedure */
                        DispatchMessage(&messages);
                }
#endif
        }
        if (mousecapture)
        {
	  //                ClipCursor(&oldclip);
                mousecapture=0;
        }
        saveadf(discname[0], 0);
        saveadf(discname[1], 1);
//        dumpregs();
        sprintf(s,"%i",(rammask+1)>>10);
        set_config_string(NULL,"mem_size",s);
        switch (model)
        {
                case 1: sprintf(s,"ARM610"); break;
                case 2: sprintf(s,"ARM710"); break;
                default: sprintf(s,"ARM7500"); break;
        }
        set_config_string(NULL,"cpu_type",s);
        if (vrammask) set_config_string(NULL,"vram_size","2");
        else          set_config_string(NULL,"vram_size","0");
        free(vram);
        free(ram);
        free(ram2);
        free(rom);

//        dumpiomd();
        fclose(arclog);
        savecmos();
        /* The program return-value is 0 - The value that PostQuitMessage() gave */
	//        return messages.wParam;
        return 0;
}


void changedisc(int drive)
{
  //        char *p;
  //      char fn[512];
  //      char start[512];

	puts("changedisc");

#if 0
        OPENFILENAME ofn;
        fn[0]=0;
        start[0]=0;
        ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "ADFS Disc Image\0*.adf\0All Files\0*.*\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = fn;
	ofn.nMaxFile = sizeof(fn);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = discname[drive];
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
        if (GetOpenFileName(&ofn))
        {
                saveadf(discname[drive], drive);
                strcpy(discname[drive],fn);
                loadadf(discname[drive], drive);
        }
#endif
}

#if 0
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        HMENU hmenu;
        switch (message)                  /* handle the messages */
        {
                case WM_COMMAND:
                hmenu=GetMenu(hwnd);
                switch (LOWORD(wParam))
                {
                        case IDM_FILE_RESET:
                        resetrpc();
                        return 0;
                        case IDM_FILE_EXIT:
                        PostQuitMessage(0);
                        return 0;
                        case IDM_DISC_LD0:
                        changedisc(ghwnd,0);
                        return 0;
                        case IDM_DISC_LD1:
                        changedisc(ghwnd,1);
                        return 0;
                        case IDM_CPU_ARM610:
                        model=1;
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_CPU_ARM710:
                        model=2;
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_CPU_ARM7500:
                        model=0;
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_RAM_4MEGS:
                        rammask=0x3FFFFF;
                        reallocmem(rammask+1);
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_RAM_8MEGS:
                        rammask=0x7FFFFF;
                        reallocmem(rammask+1);
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_RAM_16MEGS:
                        rammask=0xFFFFFF;
                        reallocmem(rammask+1);
                        resetrpc();
                        updatemenu();
                        return 0;
                }
                break;
                case WM_DESTROY:
                PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
                break;
                case WM_KILLFOCUS:
                infocus=0;
                if (mousecapture)
                {
                        ClipCursor(&oldclip);
                        mousecapture=0;
                }
                break;
                case WM_LBUTTONUP:
                if (!mousecapture)
                {
                        GetClipCursor(&oldclip);
                        GetWindowRect(hwnd,&arcclip);
                        arcclip.left+=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                        arcclip.right-=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                        arcclip.top+=GetSystemMetrics(SM_CXFIXEDFRAME)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+10;
                        arcclip.bottom-=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                        ClipCursor(&arcclip);
                        mousecapture=1;
                        updatemips=1;
                }
                break;
                default:                      /* for messages that we don't deal with */
                return DefWindowProc (hwnd, message, wParam, lParam);
    }

    return 0;
}

#endif
