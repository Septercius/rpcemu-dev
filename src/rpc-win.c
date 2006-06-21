/*RPCemu v0.5 by Tom Walker
  Windows specific stuff
  Also includes most sound emulation, since that is very Windows specific at
  the moment.
  Since I'm the only maintainer of this file, it's a complete mess.*/

int blits;
/*Uncomment for sound emulation (preliminary)
  Works okay on Windows 9x, sounds horrible on XP
  Could just be my sound card drivers though*/
#define THREADING
#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "rpcemu.h"
#include "resources.h"

int soundenabled=0;
int sndon;
int mousecapture=0;
int tlbs,flushes;
float mips,mhz,tlbsec,flushsec;
int updatemips=0;
void domips()
{
        mips=(float)inscount/1000000.0f;
        mhz=(float)cyccount/1000000.0f;
        tlbsec=(float)tlbs/1000000.0f;
        flushsec=(float)flushes;
        inscount=0;
        cyccount=0;
        tlbs=0;
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

FILE *arclog;
void rpclog(const char *format, ...)
{
   char buf[256];
//return;
   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);
   fputs(buf,arclog);
}

int drawscre=0,flyback;
int dosnd;
int samplefreq;
void sndupdate()
{
        int nextlen;
        float temp;
        iomd.state|=0x10;
        updateirqs();
        iomd.sndstat^=1;
        iomd.sndstat|=6;
        nextlen=getbufferlen()>>2;
        temp=((float)nextlen/(float)samplefreq)*1000.0f;
        nextlen=(int)temp;
        if (nextlen<10) nextlen=10;
        install_int_ex(sndupdate,MSEC_TO_TIMER(nextlen));
}

void vblupdate()
{
        drawscre=1;
}

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);
RECT oldclip,arcclip;

/*  Make the class name into a global variable  */
char szClassName[ ] = "WindowsApp";
HWND ghwnd;
HMENU menu;
int infocus;

void updatewindowsize(uint32_t x, uint32_t y)
{
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
}

void releasemousecapture()
{
        if (mousecapture)
        {
                ClipCursor(&oldclip);
                mousecapture=0;
        }
}

int dorpcreset=0;
void resetrpc()
{
        memset(ram,0,rammask+1);
        memset(vram,0,vrammask+1);
        resetcp15();
        resetarm();
        resetkeyboard();
        resetiomd();
        reseti2c();
        resetide();
        reset82c711();
}

/*Ignore _ALL_ this for now. It doesn't work very well, and doesn't even get
  started at the moment*/
int soundinited=0;
AUDIOSTREAM *as=NULL;
int sndbufsize=500;
FILE *sndfile;
int quited=0;
int samplefreq,oldfreq;
int armrunning,videorunning,soundrunning;
#define BUFLEN 2048
unsigned short sndbuffertemp[BUFLEN*4];
int nextbuf=1;

int quitsound=0;
void soundthread(PVOID pvoid)
{
        FILE *soundf;
        int offset,c,d=0;
        unsigned short *p;
        unsigned long page,start,end,temp;
        int bufferlen=1,bufferpos=0;
        soundrunning=1;
        as=NULL;
//        rpclog("Sound thread starting\n");
//        sndbufsize=500;
//        soundf=fopen("sound.pcm","wb");
        while (!quitsound)
        {
                soundstartagain:
                if (soundinited)
                {
                        offset=(iomd.sndstat&1)<<1;
                        if (!as)
                        {
                                if (sndbufsize)
                                {
                                        d=0;
                                        bufferlen=1;
                                        bufferpos=0;
                                        while ((sndbufsize*bufferlen)<BUFLEN)
                                        {
//                                                rpclog("%i %i %i\n",sndbufsize,bufferlen,sndbufsize*bufferlen);
                                                bufferlen++;
                                        }
//                                        rpclog("New sound buffer size %i freq %i bufferlen %i\n",sndbufsize,oldfreq,bufferlen);
                                        as=play_audio_stream(sndbufsize*bufferlen,16,1,oldfreq,255,127);
                                }
                                else
                                {
                                        iomd.state|=0x10;
                                        updateirqs();
                                        iomd.sndstat|=6;
                                }
                        }
                        if (as)
                        {
//                                rpclog("%i %i %i %02X\n",nextbuf,bufferpos,bufferlen,iomd.state);
                                if (bufferpos<bufferlen && !(iomd.state&0x10))
                                {
//                                        rpclog("Sound int %i\n",d);
                                        nextbuf=0;
                                        iomd.sndstat^=1;
                                        offset=(iomd.sndstat&1)<<1;
                                        page=soundaddr[offset]&0xFFFFF000;
                                        start=soundaddr[offset]&0xFF0;
                                        end=(soundaddr[offset+1]&0xFF0)+16;
                                        if (((end-start)>>2)!=sndbufsize || samplefreq!=oldfreq)
                                        {
                                                stop_audio_stream(as);
                                                as=NULL;
                                                sndbufsize=(end-start)>>2;
                                                oldfreq=samplefreq;
                                                iomd.sndstat^=1;
                                                goto soundstartagain;
                                                as=play_audio_stream(sndbufsize,16,1,44100,255,127);
                                                p=NULL;
                                                while (!p)
                                                {
                                                        p=get_audio_stream_buffer(as);
                                                }
                                        }
                                        for (c=start;c<end;c+=4)
                                        {
                                                temp=ram[((c+page)&rammask)>>2];
                                                sndbuffertemp[d++]=(temp&0xFFFF)^0x8000;
                                                sndbuffertemp[d++]=(temp>>16)^0x8000;
                                        }
                                        bufferpos++;
                                        iomd.state|=0x10;
                                        updateirqs();
                                        iomd.sndstat|=6;
                                }
                                if (bufferpos>=bufferlen)
                                {
//                                        rpclog("Buffer update %i\n",bufferlen*sndbufsize*2);
//                                        fflush(arclog);
                                        bufferpos=0;
                                        d=0;
                                        p=NULL;
                                        while (!p)
                                        {
                                                sleep(0);
                                                p=get_audio_stream_buffer(as);
                                        }
                                        memcpy(p,sndbuffertemp,bufferlen*sndbufsize*4);
                                        free_audio_stream_buffer(as);
                                        for (d=0;d<(BUFLEN*4);d++)
                                            sndbuffertemp[d]^=0x8000;
//                                        fwrite(sndbuffertemp,bufferlen*sndbufsize*4,1,soundf);
                                        d=0;
                                }
                        }
                }
                while ((iomd.state&0x10) && !quited)
                {
                        sleep(0);
                }
//                sleep(1);
        }
//        rpclog("Sound thread ending\n");
        if (as) stop_audio_stream(as);
        as=NULL;
        soundrunning=quitsound=0;
        _endthread();
}


int oldsndlen=0;

void updatesndtime(int len)
{
        float temp;
        int buflen=len;
        len>>=2;
        temp=((float)len/44100.0f)*1000.0f;
        len=(int)temp;
        if (len<2) len=2;
        if (oldsndlen!=len)
        {
                install_int_ex(sndupdate,MSEC_TO_TIMER(len));
                oldsndlen=len;
        }
}

HINSTANCE hinstance;
/*You can start paying attention again now*/
int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
        MSG messages;            /* Here messages to the application are saved */
        WNDCLASSEX wincl;        /* Data structure for the windowclass */
        char s[128];
        unsigned char d;
        FILE *f;
        int c;

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

        /* The class is registered, let's create the program*/
        ghwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           "RPCemu v0.5",       /* Title Text */
           WS_OVERLAPPEDWINDOW, /* default window */
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

        arclog=fopen("arclog.txt","wt");
        if (startrpcemu())
           return -1;
           
        atexit(releasemousecapture);

        install_int_ex(domips,MSEC_TO_TIMER(1000));
        install_int_ex(vblupdate,BPS_TO_TIMER(60));
        timeBeginPeriod(1);
        if (soundenabled)
        {
                install_sound(DIGI_AUTODETECT,0,0);
                _beginthread(soundthread,0,NULL);
        }
        else
        {
                install_int_ex(sndupdate,BPS_TO_TIMER(10));
        }
        while (!quited)
        {
                if (infocus)
                {
                        execrpcemu();
                }
                if (updatemips)
                {
                        sprintf(s,"RPCemu v0.5 - %f MIPS %i blits - %s",mips,blits,(mousecapture)?"Press CTRL-END to release mouse":"Click to capture mouse");
                        SetWindowText(ghwnd, s);
                        updatemips=0;
                }
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && mousecapture)
                {
                        ClipCursor(&oldclip);
                        mousecapture=0;
                        updatemips=1;
                }
                if (PeekMessage(&messages,NULL,0,0,PM_REMOVE))
                {
                        if (messages.message==WM_QUIT)
                           quited=1;
                        /* Translate virtual-key messages into character messages */
                        TranslateMessage(&messages);
                        /* Send message to WindowProcedure */
                        DispatchMessage(&messages);
                }
        }
        if (mousecapture)
        {
                ClipCursor(&oldclip);
                mousecapture=0;
        }
        quitsound=1;
        if (soundenabled)
        {
                while (soundrunning)
                      sleep(1);
        }
        timeEndPeriod(1);
//        dumpregs();
        endrpcemu();
        fclose(arclog);
        
        /* The program return-value is 0 - The value that PostQuitMessage() gave */
        return messages.wParam;
}

void changedisc(HWND hwnd, int drive)
{
        char *p;
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

int model2;
int mask;
int vrammask2;
int soundenabled2;
int chngram=0;

BOOL CALLBACK configdlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        HWND h;
        int c;
        int cpu;
        switch (message)
        {
                case WM_INITDIALOG:
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
                return TRUE;
                case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                        case IDOK:
                        if (soundenabled && !soundenabled2)
                        {
                                quitsound=1;
//                                while (soundrunning)
//                                      sleep(1);
//                                quitsound=0;
//                                stop_audio_stream(as);
                                install_int_ex(sndupdate,BPS_TO_TIMER(10));
                        }
                        if (soundenabled2 && !soundenabled)
                        {
                                remove_int(sndupdate);
                                install_sound(DIGI_AUTODETECT,0,0);
                                _beginthread(soundthread,0,NULL);
                        }
                        soundenabled=soundenabled2;
                        if (model!=model2 || vrammask!=vrammask2 || chngram)
                           resetrpc();
                        if (chngram)
                        {
                                rammask=mask;
                                reallocmem(rammask+1);
                        }
                        model=model2;
                        vrammask=vrammask2;
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
                        vrammask2=0x1FFFFF;
                        return TRUE;
                        
                        case RadioButton1: case RadioButton2: case RadioButton3:// case RadioButton4:
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
                        mask=(0x200000<<(LOWORD(wParam)-RadioButton5))-1;
                        for (c=RadioButton5;c<=RadioButton10;c++)
                        {
                                h=GetDlgItem(hdlg,c);
                                SendMessage(h,BM_SETCHECK,0,0);
                        }
                        h=GetDlgItem(hdlg,LOWORD(wParam));
                        SendMessage(h,BM_SETCHECK,1,0);
                        if (mask!=rammask) chngram=1;
                        else               chngram=0;
                        return TRUE;
                        
                        case CheckBox1:
                        soundenabled2^=1;
                        h=GetDlgItem(hdlg,LOWORD(wParam));
                        SendMessage(h,BM_SETCHECK,soundenabled2,0);
                        return TRUE;
                }
                break;
        }
        return FALSE;
}

LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        HMENU hmenu;
        int temp;
        char s[512];
        switch (message)                  /* handle the messages */
        {
                case WM_CREATE:
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
                        stretchmode^=1;
                        CheckMenuItem(hmenu,IDM_STRETCH,(stretchmode)?MF_CHECKED:MF_UNCHECKED);
                        return 0;
                }
                break;
                case WM_DESTROY:
                PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
                break;
                case WM_SETFOCUS:
                infocus=1;
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
