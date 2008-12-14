/*RPCemu v0.6 by Tom Walker
  Windows specific stuff
  Also includes most sound emulation, since that is very Windows specific at
  the moment.
  Since I'm the only maintainer of this file, it's a complete mess.*/

//int ins;
int mcalls=0;
#include <stdio.h>
#include <stdint.h>
#include <allegro.h>
#include <winalleg.h>
#include <process.h>
#include <commctrl.h>
#include "rpcemu.h"
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
#include "82c711.h"
#include "podules-win.h"
#include "cdrom-iso.h"
#include "cdrom-ioctl.h"

int soundenabled=0;
int mousecapture=0;
float mips = 0.0f, mhz = 0.0f, tlbsec = 0.0f, flushsec = 0.0f;
int updatemips=0;
int vsyncints=0;


void domips(void)
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

FILE *arclog;
void rpclog(const char *format, ...)
{
   char buf[1024];
//return;
   if (!arclog) arclog=fopen("rlog.txt","wt");
   if (!arclog) return;
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,arclog);
   fflush(arclog);
}

int drawscre=0;
int dosnd;
void sndupdate(void)
{
        int nextlen;
        float temp;
        iomd.state|=0x10;
        updateirqs();
        iomd.sndstat^=1;
        iomd.sndstat|=6;
        nextlen=getbufferlen()>>2;
        temp=((float)nextlen/(float)getsamplefreq())*1000.0f;
        nextlen=(int)temp;
        if (nextlen<10) nextlen=10;
        install_int_ex(sndupdate,MSEC_TO_TIMER(nextlen));
}

void vblupdate(void)
{
        drawscre++;
}

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);
RECT oldclip,arcclip;

/*  Make the class name into a global variable  */
char szClassName[ ] = "WindowsApp";
HWND ghwnd;
HMENU menu;
int infocus;

void initmenu()
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

void releasemousecapture(void)
{
        if (mousecapture)
        {
                ClipCursor(&oldclip);
                mousecapture=0;
        }
}

int quited=0;

/*Ignore _ALL_ this for now. It doesn't work very well, and doesn't even get
  started at the moment*/
/*Plus, it's just all been made obsolete! See sound.c*/
int quitblitter=0;
int blitrunning=0,vidrunning=0,soundrunning=0;
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

void _soundthread(PVOID pvoid)
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

void _closesoundthread(void)
{
        if (soundrunning)
        {
                wakeupsoundthread();
                quitblitter=1;
                while (soundrunning)
                      sleep(1);
        }
}

void vidcwakeupthread()
{
#ifdef VIDC_THREAD
        SetEvent(waitobject);
#else
        vidcthread();
#endif
}

void wakeupsoundthread()
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


HINSTANCE hinstance;
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
        menu=LoadMenu(hThisInstance,TEXT("MainMenu"));
        initmenu();
        /* The class is registered, let's create the program*/
        ghwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           "RPCemu v0.7",       /* Title Text */           
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
        if (cdromtype>2) WindowProcedure(ghwnd,WM_COMMAND,IDM_CDROM_DISABLED+cdromtype,0);
        CheckMenuItem(menu,IDM_CDROM_DISABLED+cdromtype,MF_CHECKED);
        
        CheckMenuItem(menu,IDM_STRETCH,(stretchmode)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(menu,IDM_BLITOPT,(skipblits)?MF_CHECKED:MF_UNCHECKED);
        
        if (mousehackon) CheckMenuItem(menu,IDM_MOUSE_FOL,MF_CHECKED);
        else             CheckMenuItem(menu,IDM_MOUSE_CAP,MF_CHECKED);
//        iso_open();
//        ioctl_close();
//        ioctl_gettoc();
//        ioctl_readsector();
        atexit(releasemousecapture);

        install_int_ex(domips,MSEC_TO_TIMER(1000));
//        timeBeginPeriod(1);
//        if (soundenabled)
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
        install_int_ex(vblupdate,BPS_TO_TIMER(refresh));
        drawscre=0;
        while (!quited)
        {
                if (infocus)
                {
                        execrpcemu();
                }
                if (updatemips)
                {
                        if (mousehack) sprintf(s,"RPCemu v0.7 - %f MIPS %f %i %f %i",mips,tlbsec,ins,flushsec,vsyncints);
                        else           sprintf(s,"RPCemu v0.7 - %f MIPS %f %i %f %i - %s",mips,tlbsec,ins,flushsec,vsyncints,(mousecapture)?"Press CTRL-END to release mouse":"Click to capture mouse");
                        SetWindowText(ghwnd, s);
                        updatemips=0;
                }
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && fullscreen)
                {
                        togglefullscreen(0);
                        mousecapture=0;
                }
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && mousecapture && !mousehackon)
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
//        if (soundenabled)
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

void changedisc(HWND hwnd, int drive)
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

char isoname[512];
int selectiso(HWND hwnd)
{
        char fn[512];
        char start[512];
        OPENFILENAME ofn;
        fn[0]=0;
        strcpy(start,isoname);
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
                strcpy(isoname,fn);
                return 1;
        }
        return 0;
}

int model2;
int _mask;
int vrammask2;
int soundenabled2;
int refresh2;
int chngram=0;

BOOL CALLBACK configdlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        HWND h;
        int c;
        int cpu;
        char s[10];
        switch (message)
        {
                case WM_INITDIALOG:
                        h=GetDlgItem(hdlg,Slider1);
                        SendMessage(h,TBM_SETRANGE,TRUE,MAKELONG(20/5,100/5));
                        SendMessage(h,TBM_SETPOS,TRUE,refresh/5);
                h=GetDlgItem(hdlg,Text1);
                sprintf(s,"%ihz",refresh);
                SendMessage(h,WM_SETTEXT,0,(LPARAM)s);
                h=GetDlgItem(hdlg,CheckBox1);
                SendMessage(h,BM_SETCHECK,soundenabled,0);
                if (model<2) cpu=model^1;
                else         cpu=model;
                h=GetDlgItem(hdlg,RadioButton1+cpu);
                SendMessage(h,BM_SETCHECK,1,0);
                h=GetDlgItem(hdlg,(vrammask)?RadioButton12:RadioButton11);
                SendMessage(h,BM_SETCHECK,1,0);
                switch (rammask)
                {
                        case 0x1FFFFF: h=GetDlgItem(hdlg,RadioButton5); break;
                        case 0x3FFFFF: h=GetDlgItem(hdlg,RadioButton6); break;
                        case 0x7FFFFF: h=GetDlgItem(hdlg,RadioButton7); break;
                        case 0xFFFFFF: h=GetDlgItem(hdlg,RadioButton8); break;
                        case 0x1FFFFFF: h=GetDlgItem(hdlg,RadioButton9); break;
                        case 0x3FFFFFF: h=GetDlgItem(hdlg,RadioButton10); break;
                }
                SendMessage(h,BM_SETCHECK,1,0);
                model2=model;
                vrammask2=vrammask;
                soundenabled2=soundenabled;
                refresh2=refresh;
                return TRUE;
                case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                        case IDOK:
                        if (soundenabled && !soundenabled2)
                        {
                                closesound();
                                install_int_ex(sndupdate,BPS_TO_TIMER(10));
                        }
                        if (soundenabled2 && !soundenabled)
                        {
                                initsound();
                        }
                        soundenabled=soundenabled2;
                        if (model!=model2 || vrammask!=vrammask2 || chngram)
                           resetrpc();
                        if (chngram)
                        {
                                rammask=_mask;
                                reallocmem(rammask+1);
                        }
                        model=model2;
                        vrammask=vrammask2;
                        refresh=refresh2;
                        install_int_ex(vblupdate,BPS_TO_TIMER(refresh));
                        case IDCANCEL:
                        EndDialog(hdlg,0);
                        return TRUE;
                        case RadioButton11:
                        h=GetDlgItem(hdlg,RadioButton11);
                        SendMessage(h,BM_SETCHECK,1,0);
                        h=GetDlgItem(hdlg,RadioButton12);
                        SendMessage(h,BM_SETCHECK,0,0);
                        vrammask2=0;
                        return TRUE;
                        case RadioButton12:
                        h=GetDlgItem(hdlg,RadioButton11);
                        SendMessage(h,BM_SETCHECK,0,0);
                        h=GetDlgItem(hdlg,RadioButton12);
                        SendMessage(h,BM_SETCHECK,1,0);
                        vrammask2=0x7FFFFF;
                        return TRUE;
                        
                        case RadioButton1: case RadioButton2: case RadioButton3: case RadioButton4:
                        if (model<2) cpu=model2^1;
                        else         cpu=model2;
                        h=GetDlgItem(hdlg,RadioButton1+cpu);
                        SendMessage(h,BM_SETCHECK,0,0);
                        model2=LOWORD(wParam)-RadioButton1;
                        if (model2<2) model2^=1;
                        h=GetDlgItem(hdlg,LOWORD(wParam));
                        SendMessage(h,BM_SETCHECK,1,0);
                        return TRUE;
                        
                        case RadioButton5: case RadioButton6: case RadioButton7:
                        case RadioButton8: case RadioButton9: case RadioButton10:
                        _mask=(0x200000<<(LOWORD(wParam)-RadioButton5))-1;
                        for (c=RadioButton5;c<=RadioButton10;c++)
                        {
                                h=GetDlgItem(hdlg,c);
                                SendMessage(h,BM_SETCHECK,0,0);
                        }
                        h=GetDlgItem(hdlg,LOWORD(wParam));
                        SendMessage(h,BM_SETCHECK,1,0);
                        if (_mask!=rammask) chngram=1;
                        else               chngram=0;
                        return TRUE;
                        
                        case CheckBox1:
                        soundenabled2^=1;
                        h=GetDlgItem(hdlg,LOWORD(wParam));
                        SendMessage(h,BM_SETCHECK,soundenabled2,0);
                        return TRUE;
                }
                break;
                case WM_HSCROLL:
                h=GetDlgItem(hdlg,Slider1);
                c=SendMessage(h,TBM_GETPOS,0,0);
                h=GetDlgItem(hdlg,Text1);
                sprintf(s,"%ihz",c*5);
                SendMessage(h,WM_SETTEXT,0,(LPARAM)s);
                refresh2=c*5;
                break;

        }
        return FALSE;
}

LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        HMENU hmenu;
        switch (message)                  /* handle the messages */
        {
                case WM_CREATE:
                strcpy(isoname,"");
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
                        #ifdef DYNAREC
                        DialogBox(hinstance,TEXT("ConfigureDlgDynaRec"),ghwnd,configdlgproc);
                        #else
                        DialogBox(hinstance,TEXT("ConfigureDlg"),ghwnd,configdlgproc);
                        #endif
                        return 0;
                        case IDM_STRETCH:
                        stretchmode^=1;
                        CheckMenuItem(hmenu,IDM_STRETCH,(stretchmode)?MF_CHECKED:MF_UNCHECKED);
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
                        skipblits^=1;
                        CheckMenuItem(hmenu,IDM_BLITOPT,(skipblits)?MF_CHECKED:MF_UNCHECKED);
                        return 0;
                        case IDM_CDROM_DISABLED:
                        if (cdromenabled)
                        {
                                if (MessageBox(ghwnd,"This will reset RPCemu!\nOkay to continue?","RPCemu",MB_OKCANCEL)==IDOK)
                                {
                                        cdromenabled=0;
                                        resetrpc();
                                        CheckMenuItem(hmenu,IDM_CDROM_DISABLED+cdromtype,MF_UNCHECKED);
                                        cdromtype=IDM_CDROM_DISABLED-IDM_CDROM_DISABLED;
                                        CheckMenuItem(hmenu,IDM_CDROM_DISABLED+cdromtype,MF_CHECKED);
                                }
                        }
                        return 0;
                        case IDM_CDROM_EMPTY:
                        if (!cdromenabled)
                        {
                                if (MessageBox(ghwnd,"This will reset RPCemu!\nOkay to continue?","RPCemu",MB_OKCANCEL)==IDOK)
                                {
                                        cdromenabled=1;
                                        resetrpc();
                                }
                                else
                                   return 0;
                        }
                        atapi->exit();
                        iso_init();
                        CheckMenuItem(hmenu,IDM_CDROM_DISABLED+cdromtype,MF_UNCHECKED);
                        cdromtype=IDM_CDROM_EMPTY-IDM_CDROM_DISABLED;
                        CheckMenuItem(hmenu,IDM_CDROM_DISABLED+cdromtype,MF_CHECKED);
                        return 0;
                        case IDM_CDROM_ISO:
                        if (selectiso(ghwnd))
                        {
                                if (!cdromenabled)
                                {
                                        if (MessageBox(ghwnd,"This will reset RPCemu!\nOkay to continue?","RPCemu",MB_OKCANCEL)==IDOK)
                                        {
                                                cdromenabled=1;
                                                resetrpc();
                                        }
                                        else
                                           return 0;
                                }
                                atapi->exit();
                                iso_open(isoname);
                        }
                        CheckMenuItem(hmenu,IDM_CDROM_DISABLED+cdromtype,MF_UNCHECKED);
                        cdromtype=IDM_CDROM_ISO-IDM_CDROM_DISABLED;
                        CheckMenuItem(hmenu,IDM_CDROM_DISABLED+cdromtype,MF_CHECKED);
                        return 0;
                        case IDM_MOUSE_FOL:
                        CheckMenuItem(hmenu,IDM_MOUSE_FOL,MF_CHECKED);
                        CheckMenuItem(hmenu,IDM_MOUSE_CAP,MF_UNCHECKED);
                        mousehackon=1;
                        if (mousecapture)
                        {
                                ClipCursor(&oldclip);
                                mousecapture=0;
                        }
                        return 0;
                        case IDM_MOUSE_CAP:
                        CheckMenuItem(hmenu,IDM_MOUSE_FOL,MF_UNCHECKED);
                        CheckMenuItem(hmenu,IDM_MOUSE_CAP,MF_CHECKED);
                        mousehackon=0;
                        return 0;
//                        case IDM_CDROM_G:
//                        atapi->exit();
//                        ioctl_open();
//                        return 0;
                }
                if (LOWORD(wParam)>=IDM_CDROM_REAL && LOWORD(wParam)<(IDM_CDROM_REAL+100))
                {
                        if (!cdromenabled)
                        {
                                if (MessageBox(ghwnd,"This will reset RPCemu!\nOkay to continue?","RPCemu",MB_OKCANCEL)==IDOK)
                                {
                                        cdromenabled=1;
                                        resetrpc();
                                }
                                else
                                   return 0;
                        }
                        atapi->exit();
                        ioctl_open(LOWORD(wParam)-IDM_CDROM_REAL);
                        CheckMenuItem(hmenu,IDM_CDROM_DISABLED+cdromtype,MF_UNCHECKED);
                        cdromtype=LOWORD(wParam)-IDM_CDROM_DISABLED;
                        CheckMenuItem(hmenu,IDM_CDROM_DISABLED+cdromtype,MF_CHECKED);
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
                if (!mousecapture && !fullscreen && !mousehackon)
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
