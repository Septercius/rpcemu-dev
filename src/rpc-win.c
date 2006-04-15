/*RPCemu v0.3 by Tom Walker
  Main loop*/

//#define THREADING
#include <stdio.h>
#include <allegro.h>
#include <winalleg.h>
#include "rpc.h"
#include "resources.h"

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
return;
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
//        dosnd=1;
/*        dumpsound();*/
        iomd.state|=0x10;
        updateirqs();
        iomd.sndstat^=1;
        iomd.sndstat|=6;
        nextlen=getbufferlen()>>2;
        temp=((float)nextlen/(float)samplefreq)*1000.0f;
        nextlen=(int)temp;
        if (nextlen<10) nextlen=10;
        install_int_ex(sndupdate,MSEC_TO_TIMER(nextlen));
//        rpclog("sndupdate nextlen %i\n",nextlen);
}

void vblupdate()
{
        drawscre=1;
//        iomdvsync();
//        flyback=50;
}
void vblupdate2()
{
//        drawscre=1;
//        flyback=0;
}

/*  Declare Windows procedure  */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);
RECT oldclip,arcclip;

/*  Make the class name into a global variable  */
char szClassName[ ] = "WindowsApp";
HWND ghwnd;
HMENU menu;
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
#define IDM_CPU_SA110     40033
#define IDM_RAM_4MEGS     40040
#define IDM_RAM_8MEGS     40041
#define IDM_RAM_16MEGS    40042
#define IDM_RAM_32MEGS    40043
#define IDM_RAM_64MEGS    40044
#define IDM_RAM_128MEGS   40045
#define IDM_VRAM_NONE     40050
#define IDM_VRAM_2MEGS    40051
#define IDM_CONFIG        40052

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

void makemenu()
{
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
//        AppendMenu(hpop2,MF_STRING,IDM_CPU_SA110,"&StrongARM");
        hpop=CreateMenu();
        AppendMenu(hpop,MF_POPUP,hpop2,"&CPU");
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_RAM_4MEGS,"&4 megabytes");
        AppendMenu(hpop2,MF_STRING,IDM_RAM_8MEGS,"&8 megabytes");
        AppendMenu(hpop2,MF_STRING,IDM_RAM_16MEGS,"&16 megabytes");
        AppendMenu(hpop2,MF_STRING,IDM_RAM_32MEGS,"&32 megabytes");        
        AppendMenu(hpop2,MF_STRING,IDM_RAM_64MEGS,"&64 megabytes");        
        AppendMenu(hpop2,MF_STRING,IDM_RAM_128MEGS,"1&28 megabytes");        
        AppendMenu(hpop,MF_POPUP,hpop2,"&RAM");
        hpop2=CreateMenu();
        AppendMenu(hpop2,MF_STRING,IDM_VRAM_NONE,"&None");
        AppendMenu(hpop2,MF_STRING,IDM_VRAM_2MEGS,"&2 megs");
        AppendMenu(hpop,MF_POPUP,hpop2,"&VRAM");
        AppendMenu(menu,MF_POPUP,hpop,"&Machine");
}

void releasemousecapture()
{
        if (mousecapture)
        {
                ClipCursor(&oldclip);
                mousecapture=0;
        }
}

char discname[2][260]={"boot.adf","notboot.adf"};

void updatemenu()
{
        CheckMenuItem(menu,IDM_CPU_ARM610,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_CPU_ARM710,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_CPU_ARM7500,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_CPU_SA110,MF_UNCHECKED);
        switch (model)
        {
                case 1: CheckMenuItem(menu,IDM_CPU_ARM610,MF_CHECKED);  break;
                case 2: CheckMenuItem(menu,IDM_CPU_ARM710,MF_CHECKED);  break;
                case 3: CheckMenuItem(menu,IDM_CPU_SA110,MF_CHECKED);  break;
                case 0: CheckMenuItem(menu,IDM_CPU_ARM7500,MF_CHECKED); break;
        }
        CheckMenuItem(menu,IDM_RAM_4MEGS,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_RAM_8MEGS,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_RAM_16MEGS,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_RAM_32MEGS,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_RAM_64MEGS,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_RAM_128MEGS,MF_UNCHECKED);
        switch (rammask)
        {
                case 0x1FFFFF: CheckMenuItem(menu,IDM_RAM_4MEGS,MF_CHECKED);  break;
                case 0x3FFFFF: CheckMenuItem(menu,IDM_RAM_8MEGS,MF_CHECKED);  break;
                case 0x7FFFFF: CheckMenuItem(menu,IDM_RAM_16MEGS,MF_CHECKED); break;
                case 0xFFFFFF: CheckMenuItem(menu,IDM_RAM_32MEGS,MF_CHECKED); break;
                case 0x1FFFFFF: CheckMenuItem(menu,IDM_RAM_64MEGS,MF_CHECKED); break;
                case 0x3FFFFFF: CheckMenuItem(menu,IDM_RAM_128MEGS,MF_CHECKED); break;
        }
        CheckMenuItem(menu,IDM_VRAM_NONE,MF_UNCHECKED);
        CheckMenuItem(menu,IDM_VRAM_2MEGS,MF_UNCHECKED);
        if (vrammask) CheckMenuItem(menu,IDM_VRAM_2MEGS,MF_CHECKED);
        else          CheckMenuItem(menu,IDM_VRAM_NONE,MF_CHECKED);
}

int dorpcreset=0;
#ifdef THREADING
void resetrpc()
{
        dorpcreset=1;
}
#else
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
#endif

/*Ignore _ALL_ this for now. It doesn't work very well, and doesn't even get
  started at the moment*/
int soundinited=0;
AUDIOSTREAM *as=NULL;
int sndbufsize=0;
FILE *sndfile;
/*void soundthread(PVOID pvoid)
{
        int offset,c,d;
        unsigned short *p;
        unsigned long page,start,end,temp;
        if (!sndfile) sndfile=fopen("sound.pcm","wb");
        while (1)
        {
                if (soundinited)
                {
                        offset=(iomd.sndstat&1)<<1;
                        if (!as)
                        {
                                sndbufsize=((soundaddr[offset+1]&0xFF0)-(soundaddr[offset]&0xFF0))>>2;
                                rpclog("Soundsize %i\n",sndbufsize);
                                if (sndbufsize)
                                   as=play_audio_stream(sndbufsize,16,1,44100,255,127);
                                else
                                {
                                        iomd.state|=0x10;
                                        updateirqs();
                                        iomd.sndstat|=6;
                                }
                        }
                        if (as)
                        {
                                p=get_audio_stream_buffer(as);
                                if (p)
                                {
                                        page=soundaddr[offset]&0xFFFFF000;
                                        start=soundaddr[offset]&0xFF0;
                                        end=soundaddr[offset+1]&0xFF0;
                                        if (((end-start)>>2)!=sndbufsize)
                                        {
                                                stop_audio_stream(as);
                                                sndbufsize=(end-start)>>2;
                                                rpclog("Soundsize %i\n",sndbufsize);
                                                as=play_audio_stream(sndbufsize,16,1,44100,255,127);
                                                p=NULL;
                                                while (!p)
                                                {
                                                        p=get_audio_stream_buffer(as);
                                                }
                                        }
                                        d=0;
                                        for (c=start;c<end;c+=4)
                                        {
                                                temp=ram[((c+page)&rammask)>>2];
                                                p[d++]=(temp&0xFFFF)^0x8000;
                                                p[d++]=(temp>>16)^0x8000;
                                                putc(temp&0xFF,sndfile);
                                                putc((temp>>8)&0xFF,sndfile);
                                                putc((temp>>16)&0xFF,sndfile);
                                                putc((temp>>24)&0xFF,sndfile);                                                                                                
                                        }
                                        free_audio_stream_buffer(as);
                                        rpclog("Read out buffer %i %i %03X %03X\n",iomd.sndstat,offset,start,end);
                                        iomd.state|=0x10;
                                        updateirqs();
                                        iomd.sndstat^=1;
                                        iomd.sndstat|=6;
                                }
                        }
                }
                sleep(1);
        }
}*/

int quited=0;
int armrunning,videorunning,soundrunning;

void soundthread(PVOID pvoid)
{
        int nextlen;
        float temp;
        soundrunning=1;
        while (!quited)
        {
                sleep(5);
/*                if (dosnd && sndon)
                {
                        dosnd=0;
                        dumpsound();
                }*/
                checksound();
//                if (dosnd && sndon)
//                {
/*                        nextlen=getbufferlen()>>2;
                        temp=((float)nextlen/44100.0f)*1000.0f;
                        nextlen=(int)temp;
                        if (!nextlen) nextlen=10;
                        install_int_ex(sndupdate,MSEC_TO_TIMER(nextlen));
                        rpclog("%i %i %f\n",nextlen,sndon,temp);*/
/*                        dosnd=0;                        
                        dumpsound();
                }*/
//                else if (dosnd) dosnd=0;
        }
        soundrunning=0;
        _endthread();
}

int oldsndlen=0;

void updatesndtime(int len)
{
        float temp;
        int buflen=len;
//        rpclog("Updatesndtime %i\n",len);
        len>>=2;
        temp=((float)len/44100.0f)*1000.0f;
        len=(int)temp;
//        rpclog("temp %f len %i ",temp,len);
        if (len<2) len=2;
//        rpclog("Final time %i %i\n",len,oldsndlen);
        if (oldsndlen!=len)
        {
                install_int_ex(sndupdate,MSEC_TO_TIMER(len));
//                rpclog("sndupdate time %i msecs %i\n",len,buflen);
                oldsndlen=len;
        }
}

void videothread(PVOID pvoid)
{
        videorunning=1;
        while (!quited)
        {
                if (drawscre)
                {
                        drawscre=0;
                        iomdvsync();
                        drawscr();
                        pollmouse();
                        pollkeyboard();
                }
                sleep(4);
        }
        videorunning=0;
        _endthread();
}

void armthread(PVOID pvoid)
{
        armrunning=1;
        while (!quited)
        {
                execarm(2000000);
                if (dorpcreset)
                {
                        dorpcreset=0;
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
                sleep(1);
        }
        armrunning=0;
        _endthread();
}

char HOSTFS_ROOT[512];
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
        char *p;
        unsigned char d;
        FILE *f;
        int c;
        char fn[512];

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
        makemenu();
        /* The class is registered, let's create the program*/
        ghwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           "RPCemu v0.4",       /* Title Text */
           WS_OVERLAPPEDWINDOW, /* default window */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           64+(GetSystemMetrics(SM_CXFIXEDFRAME)*2),/* The programs width */
           48+(GetSystemMetrics(SM_CYFIXEDFRAME)*2)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+2,/* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           menu,                /* No menu */
           hThisInstance,       /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

        /* Make the window visible on the screen */
        ShowWindow (ghwnd, nFunsterStil);
        win_set_window(ghwnd);
        allegro_init();
//        arclog=fopen("arclog.txt","wt");
        get_executable_name(exname,511);
        p=get_filename(exname);
        *p=0;
        append_filename(HOSTFS_ROOT,exname,"hostfs",511);
        for (c=0;c<511;c++)
        {
                if (HOSTFS_ROOT[c]=='\\')
                   HOSTFS_ROOT[c]='/';
        }
        append_filename(fn,exname,"rpc.cfg",511);
        set_config_file(fn);
        p=get_config_string(NULL,"mem_size",NULL);
        if (!p || !strcmp(p,"4"))  rammask=0x1FFFFF;
        else if (!strcmp(p,"8"))   rammask=0x3FFFFF;
        else if (!strcmp(p,"32"))  rammask=0xFFFFFF;
        else if (!strcmp(p,"64"))  rammask=0x1FFFFFF;
        else if (!strcmp(p,"128")) rammask=0x3FFFFFF;
        else                       rammask=0x7FFFFF;
        p=get_config_string(NULL,"vram_size",NULL);
        if (!p || !strcmp(p,"0"))  vrammask=0;
        else                       vrammask=0x1FFFFF;
        p=get_config_string(NULL,"cpu_type",NULL);
        if (!p || !strcmp(p,"ARM610")) model=1;
        else if (!strcmp(p,"ARM710"))  model=2;
        else if (!strcmp(p,"SA110"))   model=3;
        else                           model=0;
        initmem();
        if (loadroms())
        {
                MessageBox(NULL,"RiscOS ROMs missing!","RiscOS ROMs missing!",MB_OK);
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
        initsound();
        install_int_ex(domips,MSEC_TO_TIMER(1000));
        install_int_ex(vblupdate,BPS_TO_TIMER(60));
        install_int_ex(sndupdate,BPS_TO_TIMER(10));
//        install_sound(DIGI_AUTODETECT,MIDI_NONE,0);
        reallocmem(rammask+1);
//        model=0;
//        vrammask=0x1FFFFF;
//        rammask=0x3FFFFF;
        updatemenu();
//        rest(5);
//        install_int_ex(vblupdate2,BPS_TO_TIMER(60));
        timeBeginPeriod(1);
                #ifdef THREADING
                _beginthread(armthread,0,NULL);
                _beginthread(videothread,0,NULL);
                _beginthread(soundthread,0,NULL);
                #endif
        while (!quited)
        {
                #ifndef THREADING
                if (infocus)
                {
                        execarm(80000);
                        if (drawscre)
                        {
                                drawscre=0;
                                iomdvsync();
                                drawscr();
                                pollmouse();
                                pollkeyboard();
                        }
                }
                #else
                sleep(1);
                #endif
                if (updatemips)
                {
                        sprintf(s,"RPCemu v0.4 - %f MIPS - %s",mips,(mousecapture)?"Press CTRL-END to release mouse":"Click to capture mouse");
                        SetWindowText(ghwnd, s);
                        updatemips=0;
                }
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && mousecapture)
                {
                        ClipCursor(&oldclip);
                        mousecapture=0;
                        updatemips=1;
                }
/*This is a useful method of loading stuff into RAM to be saved to hard disc.
  It's not the most trivial of methods though, and can happen accidently, so I've
  commented it out for now*/
/*                if (key[KEY_F11])
                {
                        f=fopen("klf","rb");
                        c=0x8000;
                        while (!feof(f))
                        {
                                d=getc(f);
                                writememb(c,d);
                                c++;
                        }
                        error("Len %08X",ftell(f));
                        fclose(f);
                }*/
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
        #ifdef THREADING
        while (armrunning || videorunning || soundrunning)
              sleep(1);
        #endif
        timeEndPeriod(1);
        saveadf(discname[0], 0);
        saveadf(discname[1], 1);
//        dumpregs();
        sprintf(s,"%i",((rammask+1)>>20)<<1);
        set_config_string(NULL,"mem_size",s);
        switch (model)
        {
                case 1: sprintf(s,"ARM610"); break;
                case 2: sprintf(s,"ARM710"); break;
                case 3: sprintf(s,"SA110"); break;
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
//        fclose(arclog);
        savecmos();
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
                        case IDM_CPU_SA110:
                        model=3;
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_RAM_4MEGS:
                        rammask=0x1FFFFF;
                        reallocmem(rammask+1);
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_RAM_8MEGS:
                        rammask=0x3FFFFF;
                        reallocmem(rammask+1);
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_RAM_16MEGS:
                        rammask=0x7FFFFF;
                        reallocmem(rammask+1);
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_RAM_32MEGS:
                        rammask=0xFFFFFF;
                        reallocmem(rammask+1);
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_RAM_64MEGS:
                        rammask=0x1FFFFFF;
                        reallocmem(rammask+1);
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_RAM_128MEGS:
                        rammask=0x3FFFFFF;
                        reallocmem(rammask+1);
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_VRAM_NONE:
                        vrammask=0;
                        resetrpc();
                        updatemenu();
                        return 0;
                        case IDM_VRAM_2MEGS:
                        vrammask=0x1FFFFF;
                        resetrpc();
                        updatemenu();
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
