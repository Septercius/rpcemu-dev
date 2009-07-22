/*RPCemu v0.6 by Tom Walker
  Windows specific stuff
  Also includes most sound emulation, since that is very Windows specific at
  the moment.
  Since I'm the only maintainer of this file, it's a complete mess.*/

//int ins;
#include <stdio.h>
#include <stdint.h>
#include <allegro.h>
#include <winalleg.h>
#include <process.h>
#include <commctrl.h>
#include "rpcemu.h"
#include "config.h"
#include "resources.h"
#include "vidc20.h"
#include "keyboard.h"
#include "sound.h"
#include "mem.h"
#include "iomd.h"
#include "ide.h"
#include "arm.h"
#include "cmos.h"
#include "cp15.h"
#include "fdc.h"
#include "podules-win.h"
#include "cdrom-iso.h"
#include "cdrom-ioctl.h"

static float mips = 0.0f, mhz = 0.0f, tlbsec = 0.0f, flushsec = 0.0f;
static int updatemips=0;
static int vsyncints=0;


static void domips(void)
{
        mips=(float)inscount/1000000.0f;
        inscount=0;
        mhz=(float)cyccount/1000000.0f;
        cyccount=0;
        tlbsec=(float)tlbs/1000000.0f;
        tlbs=0;
        flushsec=(float)flushes;
        flushes=0;
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
void fatal(const char *format, ...)
{
   char buf[256];

   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   MessageBox(NULL,buf,"RPCemu error",MB_OK);;

   abort();
}

static void sndupdate(void)
{
        int nextlen;
        float temp;
        iomd.irqdma.status |= 0x10;
        updateirqs();
        iomd.sndstat^=1;
        iomd.sndstat|=6;
        nextlen=getbufferlen()>>2;
        temp=((float)nextlen/(float)getsamplefreq())*1000.0f;
        nextlen=(int)temp;
        if (nextlen<10) nextlen=10;
        install_int_ex(sndupdate,MSEC_TO_TIMER(nextlen));
}

static void vblupdate(void)
{
        drawscre++;
}

/*  Declare Windows procedure  */
static LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);
static RECT oldclip, arcclip;

/*  Make the class name into a global variable  */
static char szClassName[ ] = "WindowsApp";
static HWND ghwnd;
static HMENU menu;
int infocus;

/**
 * Fill in menu with CDROM links based on real windows drives
 */
static void initmenu(void)
{
        int c;
        HMENU m;
        char s[32];
        m=GetSubMenu(menu,2); /*Settings*/
        m=GetSubMenu(m,4); /*CD-ROM*/
        for (c='A';c<='Z';c++)
        {
                sprintf(s,"%c:\\",c);
                if (GetDriveType(s)==DRIVE_CDROM)
                {
                        sprintf(s,"CD-ROM Drive %c:",c);
                        AppendMenu(m,MF_STRING,IDM_CDROM_REAL+c,s);
                }
        }
}

void updatewindowsize(uint32_t x, uint32_t y)
{
        RECT r;
//        rpclog("Updatewindowsize %i %i %i\n",x,y,ins);
        GetWindowRect(ghwnd,&r);
        MoveWindow(ghwnd,r.left,r.top,
                     x+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),
                     y+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION),
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
}

static void releasemousecapture(void)
{
        if (mousecapture)
        {
                ClipCursor(&oldclip);
                mousecapture=0;
        }
}

/*Ignore _ALL_ this for now. It doesn't work very well, and doesn't even get
  started at the moment*/
/*Plus, it's just all been made obsolete! See sound.c*/
static int quitblitter=0;
static int blitrunning=0,soundrunning=0;
/* static int vidrunning=0 */
static HANDLE waitobject,soundobject;
static CRITICAL_SECTION vidcmutex;

void vidcthreadrunner(PVOID pvoid)
{
        blitrunning=1;
        while (!quitblitter)
        {
                WaitForSingleObject(waitobject,INFINITE);
                EnterCriticalSection(&vidcmutex);
                if (!quitblitter)
                   vidcthread();
                LeaveCriticalSection(&vidcmutex);
        }
        blitrunning=0;
}

void vidcendthread(void)
{
#ifdef VIDC_THREAD
        if (blitrunning)
        {
                quitblitter=1;
                vidcwakeupthread();
                while (blitrunning)
                      sleep(1);
        }
        DeleteCriticalSection(&vidcmutex);
#endif
}

static void _soundthread(PVOID pvoid)
{
        int c;
//        timeBeginPeriod(1);
        soundrunning=1;
        while (!quitblitter)
        {
                WaitForSingleObject(soundobject,INFINITE);
                if (!quitblitter)
                {
                        c=1;
                        while (c)
                        {
                                c=updatesoundbuffer();
                        }
                }
//                sleep(0);
        }
        soundrunning=0;
}

static void _closesoundthread(void)
{
        if (soundrunning)
        {
                wakeupsoundthread();
                quitblitter=1;
                while (soundrunning)
                      sleep(1);
        }
}

void vidcwakeupthread(void)
{
#ifdef VIDC_THREAD
        SetEvent(waitobject);
#else
        vidcthread();
#endif
}

void wakeupsoundthread(void)
{
        SetEvent(soundobject);
}

void vidcstartthread(void)
{
    HANDLE bltthread;
#ifdef VIDC_THREAD
    waitobject=CreateEvent(NULL, FALSE, FALSE, NULL);
    bltthread=(HANDLE)_beginthread(vidcthreadrunner,0,NULL);
    SetThreadPriority(bltthread,THREAD_PRIORITY_TIME_CRITICAL);
    InitializeCriticalSectionAndSpinCount(&vidcmutex,0);
#endif
}

int vidctrymutex(void)
{
#ifdef VIDC_THREAD
    return TryEnterCriticalSection(&vidcmutex);
#else
    return 1;
#endif
}

void vidcreleasemutex(void)
{
#ifdef VIDC_THREAD
    LeaveCriticalSection(&vidcmutex);
#endif
}


static HINSTANCE hinstance;
/*You can start paying attention again now*/
int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
        MSG messages = {0};     /* Here messages to the application are saved */
        WNDCLASSEX wincl;        /* Data structure for the windowclass */
        char s[128];
        HANDLE soundthread;

        hinstance=hThisInstance;
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

        /* Load Menu from resources file */
        menu=LoadMenu(hThisInstance,TEXT("MainMenu"));

        /* Add in CDROM links to the settings menu dynamically */
        initmenu();

        /* The class is registered, let's create the program*/
        ghwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           "RPCemu v" VERSION,      /* Title Text */
/*           WS_OVERLAPPEDWINDOW,*/ /* default window */           
           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, /* overlapped window with no sizeing frame */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           64+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),/* The programs width */
           48+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+2,/* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           menu, /* No menu */
           hThisInstance,       /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

        /* Make the window visible on the screen */
        ShowWindow (ghwnd, nFunsterStil);
        win_set_window(ghwnd);
        allegro_init();
        install_keyboard();
        install_timer();
        install_mouse();
infocus=0;
//        arclog=fopen("arclog.txt","wt");
        if (startrpcemu())
           return -1;
        opendlls();
        if (config.cdromtype > 2) WindowProcedure(ghwnd, WM_COMMAND, IDM_CDROM_DISABLED + config.cdromtype, 0);
        CheckMenuItem(menu, IDM_CDROM_DISABLED + config.cdromtype, MF_CHECKED);
        
        CheckMenuItem(menu, IDM_STRETCH, config.stretchmode ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(menu, IDM_BLITOPT, config.skipblits   ? MF_CHECKED : MF_UNCHECKED);
        
        if (config.mousehackon) CheckMenuItem(menu,IDM_MOUSE_FOL,MF_CHECKED);
        else             CheckMenuItem(menu,IDM_MOUSE_CAP,MF_CHECKED);
//        iso_open();
//        ioctl_close();
//        ioctl_gettoc();
//        ioctl_readsector();
        atexit(releasemousecapture);

        install_int_ex(domips,MSEC_TO_TIMER(1000));
//        timeBeginPeriod(1);
//        if (config.soundenabled)
//        {
                initsound();
//                install_sound(DIGI_AUTODETECT,0,0);
//                _beginthread(soundthread,0,NULL);
/*        }
        else
        {
                install_int_ex(sndupdate,BPS_TO_TIMER(10));
        }*/
                soundobject=CreateEvent(NULL, FALSE, FALSE, NULL);
                soundthread=(HANDLE)_beginthread(_soundthread,0,NULL);
                atexit(_closesoundthread);
//        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
infocus=1;
        install_int_ex(vblupdate, BPS_TO_TIMER(config.refresh));
        drawscre=0;
        while (!quited)
        {
                if (infocus)
                {
                        execrpcemu();
                }
                if (updatemips)
                {
                        if (mousehack) sprintf(s, "RPCemu v" VERSION " - %f MIPS %f %i %f %i",mips,tlbsec,ins,flushsec,vsyncints);
                        else           sprintf(s, "RPCemu v" VERSION " - %f MIPS %f %i %f %i - %s",mips,tlbsec,ins,flushsec,vsyncints,(mousecapture)?"Press CTRL-END to release mouse":"Click to capture mouse");
                        SetWindowText(ghwnd, s);
                        updatemips=0;
                }
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && fullscreen)
                {
                        togglefullscreen(0);
                        mousecapture=0;
                }
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && mousecapture && !config.mousehackon)
                {
                        ClipCursor(&oldclip);
                        mousecapture=0;
                        updatemips=1;
                }
                if (PeekMessage(&messages,NULL,0,0,PM_REMOVE))
                {
                        if (messages.message==WM_QUIT)
                        {
                                quited=1;
//                                closevideo();
                        }
                        /* Translate virtual-key messages into character messages */
                        TranslateMessage(&messages);
                        /* Send message to WindowProcedure */
                        DispatchMessage(&messages);
                }
        }
        infocus=0;
        if (mousecapture)
        {
                ClipCursor(&oldclip);
                mousecapture=0;
        }
        #ifdef BLITTER_THREAD
        quitblitter=1;
        wakeupblitterthread();
        while (blitrunning)
              sleep(1);
//        if (config.soundenabled)
//        {
                wakeupsoundthread();
                while (soundrunning)
                      sleep(1);
//        }
//        while (vidrunning)
//              sleep(1);
        #endif
//        closevideo();
//        timeEndPeriod(1);
        dumpregs();
        endrpcemu();
//        fclose(arclog);
        
        /* The program return-value is 0 - The value that PostQuitMessage() gave */
        return messages.wParam;
}

static void changedisc(HWND hwnd, int drive)
{
        char fn[512];
        char start[512];
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
}

static int selectiso(HWND hwnd)
{
        char fn[512];
        char start[512];
        OPENFILENAME ofn;
        fn[0]=0;
        strcpy(start, config.isoname);
//        start[0]=0;
        ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "ISO CD-ROM Image\0*.iso\0All Files\0*.*\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = fn;
	ofn.nMaxFile = sizeof(fn);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = start;//discname[drive];
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
                strcpy(config.isoname, fn);
                return 1;
        }
        return 0;
}

static int _mask;
static int vrammask2;
static int soundenabled2;
static int refresh2;
static int chngram = 0;

static BOOL CALLBACK configdlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        static CPUModel model2 = CPUModel_ARM7500;
        HWND h;
        int c;
        int cpu;
        char s[10];
        switch (message)
        {
        case WM_INITDIALOG:
                h = GetDlgItem(hdlg, Slider_Refresh);
                SendMessage(h,TBM_SETRANGE,TRUE,MAKELONG(20/5,100/5));
                SendMessage(h, TBM_SETPOS, TRUE, config.refresh / 5);
                h = GetDlgItem(hdlg, Text_Refresh);
                sprintf(s, "%ihz", config.refresh);
                SendMessage(h,WM_SETTEXT,0,(LPARAM)s);

                /* Set Sound enabled */
                h = GetDlgItem(hdlg, CheckBox_Sound);
                SendMessage(h, BM_SETCHECK, config.soundenabled, 0);

                /* Set CPU model */
                /* The CPUModel list and Dialog items are in a different order */
                switch (config.model) {
                case CPUModel_ARM7500:
                        cpu = 1; break;
                case CPUModel_ARM610:
                        cpu = 0; break;
                case CPUModel_ARM710:
                        cpu = 2; break;
                case CPUModel_SA110:
                        cpu = 3; break;
                default:
                        fprintf(stderr, "configdlgproc(): unknown CPU model %d\n", config.model);
                        exit(EXIT_FAILURE);
                }
                h = GetDlgItem(hdlg, RadioButton_ARM610 + cpu);
                SendMessage(h,BM_SETCHECK,1,0);

                /* Set VRAM */
                h = GetDlgItem(hdlg, config.vrammask ? RadioButton_VRAM_2 : RadioButton_VRAM_0);
                SendMessage(h,BM_SETCHECK,1,0);

                /* Set RAM Size */
                switch (config.rammask)
                {
                case 0x1FFFFF:  h = GetDlgItem(hdlg, RadioButton_Mem_4); break;
                case 0x3FFFFF:  h = GetDlgItem(hdlg, RadioButton_Mem_8); break;
                case 0x7FFFFF:  h = GetDlgItem(hdlg, RadioButton_Mem_16); break;
                case 0xFFFFFF:  h = GetDlgItem(hdlg, RadioButton_Mem_32); break;
                case 0x1FFFFFF: h = GetDlgItem(hdlg, RadioButton_Mem_64); break;
                case 0x3FFFFFF: h = GetDlgItem(hdlg, RadioButton_Mem_128); break;
                }
                SendMessage(h,BM_SETCHECK,1,0);

                model2        = config.model;
                vrammask2     = config.vrammask;
                soundenabled2 = config.soundenabled;
                refresh2      = config.refresh;
                return TRUE;

        case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                case IDOK:
                        if (config.soundenabled && !soundenabled2)
                        {
                                closesound();
                                install_int_ex(sndupdate,BPS_TO_TIMER(10));
                        }
                        if (soundenabled2 && !config.soundenabled)
                        {
                                initsound();
                        }
                        config.soundenabled = soundenabled2;
                        if (config.model != model2 || config.vrammask != vrammask2 || chngram)
                           resetrpc();
                        if (chngram)
                        {
                                config.rammask = _mask;
                                mem_reset(config.rammask + 1);
                        }
                        config.model = model2;
                        config.vrammask = vrammask2;
                        config.refresh = refresh2;
                        install_int_ex(vblupdate, BPS_TO_TIMER(config.refresh));
                        EndDialog(hdlg,0);
                        return TRUE;

                case IDCANCEL:
                        EndDialog(hdlg,0);
                        return TRUE;

                /* VRAM None */
                case RadioButton_VRAM_0:
                        h = GetDlgItem(hdlg, RadioButton_VRAM_0);
                        SendMessage(h,BM_SETCHECK,1,0);
                        h = GetDlgItem(hdlg, RadioButton_VRAM_2);
                        SendMessage(h,BM_SETCHECK,0,0);
                        vrammask2=0;
                        return TRUE;

                /* VRAM 2MB */
                case RadioButton_VRAM_2:
                        h = GetDlgItem(hdlg, RadioButton_VRAM_0);
                        SendMessage(h,BM_SETCHECK,0,0);
                        h = GetDlgItem(hdlg, RadioButton_VRAM_2);
                        SendMessage(h,BM_SETCHECK,1,0);
                        vrammask2=0x7FFFFF;
                        return TRUE;
                        
                /* CPU Type radio buttons */
                case RadioButton_ARM610:
                case RadioButton_ARM7500:
                case RadioButton_ARM710:
                case RadioButton_SA110:
                        /* The model enum and the dialog IDs are in different orders */

                        /* Clear previous CPU model choice */
                        switch (config.model) {
                        case CPUModel_ARM7500:
                                cpu = 1; break;
                        case CPUModel_ARM610:
                                cpu = 0; break;
                        case CPUModel_ARM710:
                                cpu = 2; break;
                        case CPUModel_SA110:
                                cpu = 3; break;
                        default:
                                fprintf(stderr, "configdlgproc(): unknown CPU model %d\n", config.model);
                                exit(EXIT_FAILURE);
                        }

                        h = GetDlgItem(hdlg, RadioButton_ARM610 + cpu);
                        SendMessage(h, BM_SETCHECK, 0, 0);

                        /* Set new CPU model */
                        switch (LOWORD(wParam) - RadioButton_ARM610) {
                        case 0:
                                model2 = CPUModel_ARM610; break;
                        case 1:
                                model2 = CPUModel_ARM7500; break;
                        case 2:
                                model2 = CPUModel_ARM710; break;
                        case 3:
                                model2 = CPUModel_SA110; break;
                        default:
                                fprintf(stderr, "configdlgproc(): unknown dialog item for CPUModel %d\n",
                                        LOWORD(wParam) - RadioButton_ARM610);
                                exit(EXIT_FAILURE);
                        }

                        h = GetDlgItem(hdlg, LOWORD(wParam));
                        SendMessage(h, BM_SETCHECK, 1, 0);
                        return TRUE;
                        
                /* Memory selection radio buttons */
                case RadioButton_Mem_4:
                case RadioButton_Mem_8:
                case RadioButton_Mem_16:
                case RadioButton_Mem_32:
                case RadioButton_Mem_64:
                case RadioButton_Mem_128:
                        _mask = (0x200000 << (LOWORD(wParam) - RadioButton_Mem_4)) - 1;
                        for (c = RadioButton_Mem_4; c <= RadioButton_Mem_128; c++)
                        {
                                h=GetDlgItem(hdlg,c);
                                SendMessage(h,BM_SETCHECK,0,0);
                        }
                        h=GetDlgItem(hdlg,LOWORD(wParam));
                        SendMessage(h,BM_SETCHECK,1,0);
                        if (_mask != config.rammask) chngram=1;
                        else               chngram=0;
                        return TRUE;
                        
                /* Sound */
                case CheckBox_Sound:
                        soundenabled2^=1;
                        h=GetDlgItem(hdlg,LOWORD(wParam));
                        SendMessage(h,BM_SETCHECK,soundenabled2,0);
                        return TRUE;
                }
                break;

        case WM_HSCROLL:
                h = GetDlgItem(hdlg, Slider_Refresh);
                c=SendMessage(h,TBM_GETPOS,0,0);
                h = GetDlgItem(hdlg, Text_Refresh);
                sprintf(s,"%ihz",c*5);
                SendMessage(h,WM_SETTEXT,0,(LPARAM)s);
                refresh2=c*5;
                break;

        }
        return FALSE;
}

static LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        HMENU hmenu;
        switch (message)                  /* handle the messages */
        {
        case WM_CREATE:
                strcpy(config.isoname, "");
//                _beginthread(soundthread,0,NULL);
                return 0;

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

                case IDM_CONFIG:
                        DialogBox(hinstance,TEXT("ConfigureDlg"),ghwnd,configdlgproc);
                        return 0;

                case IDM_STRETCH:
                        config.stretchmode ^= 1;
                        CheckMenuItem(hmenu, IDM_STRETCH, config.stretchmode ? MF_CHECKED : MF_UNCHECKED);
                        return 0;

                case IDM_FULLSCR:
                        if (mousecapture)
                        {
                                ClipCursor(&oldclip);
                                mousecapture=0;
                        }
                        togglefullscreen(1);
                        return 0;

                case IDM_BLITOPT:
                        config.skipblits ^= 1;
                        CheckMenuItem(hmenu, IDM_BLITOPT, config.skipblits ? MF_CHECKED : MF_UNCHECKED);
                        return 0;

                case IDM_CDROM_DISABLED:
                        if (config.cdromenabled)
                        {
                                if (MessageBox(ghwnd,"This will reset RPCemu!\nOkay to continue?","RPCemu",MB_OKCANCEL)==IDOK)
                                {
                                        config.cdromenabled = 0;
                                        resetrpc();
                                        CheckMenuItem(hmenu, IDM_CDROM_DISABLED + config.cdromtype, MF_UNCHECKED);
                                        config.cdromtype = IDM_CDROM_DISABLED - IDM_CDROM_DISABLED;
                                        CheckMenuItem(hmenu, IDM_CDROM_DISABLED + config.cdromtype, MF_CHECKED);
                                }
                        }
                        return 0;

                case IDM_CDROM_EMPTY:
                        if (!config.cdromenabled)
                        {
                                if (MessageBox(ghwnd,"This will reset RPCemu!\nOkay to continue?","RPCemu",MB_OKCANCEL)==IDOK)
                                {
                                        config.cdromenabled = 1;
                                        resetrpc();
                                }
                                else
                                   return 0;
                        }
                        atapi->exit();
                        iso_init();
                        CheckMenuItem(hmenu, IDM_CDROM_DISABLED + config.cdromtype, MF_UNCHECKED);
                        config.cdromtype = IDM_CDROM_EMPTY - IDM_CDROM_DISABLED;
                        CheckMenuItem(hmenu, IDM_CDROM_DISABLED + config.cdromtype, MF_CHECKED);
                        return 0;

                case IDM_CDROM_ISO:
                        if (selectiso(ghwnd))
                        {
                                if (!config.cdromenabled)
                                {
                                        if (MessageBox(ghwnd,"This will reset RPCemu!\nOkay to continue?","RPCemu",MB_OKCANCEL)==IDOK)
                                        {
                                                config.cdromenabled = 1;
                                                resetrpc();
                                        }
                                        else
                                           return 0;
                                }
                                atapi->exit();
                                iso_open(config.isoname);
                        }
                        CheckMenuItem(hmenu, IDM_CDROM_DISABLED + config.cdromtype, MF_UNCHECKED);
                        config.cdromtype = IDM_CDROM_ISO - IDM_CDROM_DISABLED;
                        CheckMenuItem(hmenu, IDM_CDROM_DISABLED + config.cdromtype, MF_CHECKED);
                        return 0;

                case IDM_MOUSE_FOL:
                        CheckMenuItem(hmenu,IDM_MOUSE_FOL,MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_MOUSE_CAP,MF_UNCHECKED);
                        config.mousehackon = 1;
                        if (mousecapture)
                        {
                                ClipCursor(&oldclip);
                                mousecapture=0;
                        }
                        return 0;

                case IDM_MOUSE_CAP:
                        CheckMenuItem(hmenu,IDM_MOUSE_FOL,MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_MOUSE_CAP,MF_CHECKED);
                        config.mousehackon = 0;
                        return 0;
                }

                if (LOWORD(wParam)>=IDM_CDROM_REAL && LOWORD(wParam)<(IDM_CDROM_REAL+100))
                {
                        if (!config.cdromenabled)
                        {
                                if (MessageBox(ghwnd,"This will reset RPCemu!\nOkay to continue?","RPCemu",MB_OKCANCEL)==IDOK)
                                {
                                        config.cdromenabled = 1;
                                        resetrpc();
                                }
                                else
                                   return 0;
                        }
                        atapi->exit();
                        ioctl_open(LOWORD(wParam)-IDM_CDROM_REAL);
                        CheckMenuItem(hmenu, IDM_CDROM_DISABLED + config.cdromtype, MF_UNCHECKED);
                        config.cdromtype = LOWORD(wParam) - IDM_CDROM_DISABLED;
                        CheckMenuItem(hmenu, IDM_CDROM_DISABLED + config.cdromtype, MF_CHECKED);
                        return 0;
                }
                break;

        case WM_DESTROY:
                closevideo();
                infocus=0;
                PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
                break;

        case WM_SETFOCUS:
                infocus=1;
                resetbuffer();
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
                if (!mousecapture && !fullscreen && !config.mousehackon)
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
