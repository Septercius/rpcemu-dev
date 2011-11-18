/*RPCemu v0.6 by Tom Walker
  Windows specific stuff
  Also includes most sound emulation, since that is very Windows specific at
  the moment.
  Since I'm the only maintainer of this file, it's a complete mess.*/

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

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
#include "fdc.h"
#include "podules-win.h"
#include "cdrom-iso.h"
#include "cdrom-ioctl.h"
#include "network.h"

extern void sig_io(int sig);

/*  Declare Windows procedure  */
static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
static RECT oldclip; /**< Used to store the clip box of the cursor before we
                          enter 'mouse capture' mode, so it can be restored later */

static HWND ghwnd;
static HMENU menu;
int handle_sigio; /**< bool to indicate new network data is received */

static int quitblitter=0;
static int blitrunning=0,soundrunning=0;
static HANDLE waitobject,soundobject;
static CRITICAL_SECTION vidcmutex;

static int chngram = 0;

static Config chosen_config; /**< Temp store of config the user chose in the configuration dialog */

static HINSTANCE hinstance; /**< Handle to current program instance */

void error(const char *format, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	rpclog("ERROR: %s\n", buf);
	MessageBox(NULL, buf, "RPCEmu error", MB_OK);
}

void fatal(const char *format, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);
	rpclog("FATAL: %s\n", buf);
	MessageBox(NULL, buf, "RPCEmu error", MB_OK);

	exit(EXIT_FAILURE);
}

/**
 * Log details about the current Operating System version.
 *
 * Called during program start-up.
 */
void
rpcemu_log_os(void)
{
	typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
	typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

	OSVERSIONINFOEX osvi;
	SYSTEM_INFO si;
	PGNSI pGNSI;

	rpclog("OS: Microsoft Windows\n");

	memset(&osvi, 0, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (!GetVersionEx((OSVERSIONINFO *) &osvi)) {
		rpclog("OS: Failed GetVersionEx()\n");
		return;
	}

	pGNSI = (PGNSI) GetProcAddress(GetModuleHandle("kernel32.dll"),
	                               "GetNativeSystemInfo");
	if (pGNSI != NULL) {
		pGNSI(&si);
	} else {
		GetSystemInfo(&si);
	}

	rpclog("OS: PlatformId = %ld\n", osvi.dwPlatformId);
	rpclog("OS: MajorVersion = %ld\n", osvi.dwMajorVersion);
	rpclog("OS: MinorVersion = %ld\n", osvi.dwMinorVersion);

	/* If earlier than Windows 2000, log no more detail */
	if (osvi.dwPlatformId != VER_PLATFORM_WIN32_NT || osvi.dwMajorVersion < 5) {
		return;
	}

	rpclog("OS: ProductType = %d\n",  osvi.wProductType);
	rpclog("OS: SuiteMask = 0x%x\n",  osvi.wSuiteMask);
	rpclog("OS: ServicePackMajor = %d\n", osvi.wServicePackMajor);
	rpclog("OS: ServicePackMinor = %d\n", osvi.wServicePackMinor);

	rpclog("OS: ProcessorArchitecture = %d\n", si.wProcessorArchitecture);

	rpclog("OS: SystemMetricsServerR2 = %d\n", GetSystemMetrics(SM_SERVERR2));

	if (osvi.dwMajorVersion >= 6) {
		PGPI pGPI;
		DWORD dwType;

		pGPI = (PGPI) GetProcAddress(GetModuleHandle("kernel32.dll"),
		                             "GetProductInfo");
		pGPI(osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);
		rpclog("OS: ProductInfoType = %ld\n", dwType);
	}
}

static void vblupdate(void)
{
        drawscre++;
}

/**
 * Fill in menu with CDROM links based on real windows drives
 */
static void initmenu(void)
{
        int c;
        HMENU m;
        char s[32];

	m = GetSubMenu(menu, 2); /* Settings */
	m = GetSubMenu(m, 7); /* CD-ROM */

        /* Loop through each Windows drive letter and test to see if
           it's a CDROM */
        for (c='A';c<='Z';c++)
        {
                sprintf(s,"%c:\\",c);
                if (GetDriveType(s)==DRIVE_CDROM)
                {
                        sprintf(s, "Host CD/DVD Drive (%c:)", c);
                        AppendMenu(m,MF_STRING,IDM_CDROM_REAL+c,s);
                }
        }
}

/**
 * Called from VIDC code when the RISC OS mode has changed.
 * Update windows size to hold it.
 *
 * @param x X size in pixels
 * @param y Y size in pixels
 */
void updatewindowsize(uint32_t x, uint32_t y)
{
	RECT r;

	GetWindowRect(ghwnd, &r);
	MoveWindow(ghwnd, r.left, r.top,
	           x + (GetSystemMetrics(SM_CXFIXEDFRAME) * 2),
	           y + (GetSystemMetrics(SM_CYFIXEDFRAME) * 2) + GetSystemMetrics(SM_CYMENU) + GetSystemMetrics(SM_CYCAPTION),
	           TRUE);
        if (mousecapture)
        {
                RECT arcclip;

                ClipCursor(&oldclip);
                GetWindowRect(ghwnd,&arcclip);
                arcclip.left+=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                arcclip.right-=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                arcclip.top+=GetSystemMetrics(SM_CXFIXEDFRAME)+GetSystemMetrics(SM_CYMENUSIZE)+GetSystemMetrics(SM_CYCAPTION)+10;
                arcclip.bottom-=GetSystemMetrics(SM_CXFIXEDFRAME)+10;
                ClipCursor(&arcclip);
        }
}

/**
 * Allow the mouse to once again travel the whole
 * screen, restores the mouse clip box to it's previous
 * state.
 */
static void releasemousecapture(void)
{
        if (mousecapture)
        {
                ClipCursor(&oldclip);
                mousecapture=0;
        }
}

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
        if (blitrunning)
        {
                quitblitter=1;
                vidcwakeupthread();
                while (blitrunning)
                      sleep(1);
        }
        DeleteCriticalSection(&vidcmutex);
}

/**
 * Function called in sound thread to block
 * on waiting for sound data and trigger copying
 * it to Allegro's sound output buffer
 */
static void sound_thread_function(PVOID pvoid)
{
        soundrunning=1;
        while (!quitblitter)
        {
                WaitForSingleObject(soundobject,INFINITE);
                if (!quitblitter)
                {
                        sound_buffer_update();
                }
//                sleep(0);
        }
        soundrunning=0;
}

/**
 * Called on program startup. Create a thread for copying sound
 * data into Allegro's sound output buffer
 */
void sound_thread_start(void)
{
	HANDLE soundthread;

	soundobject = CreateEvent(NULL, FALSE, FALSE, NULL);
	soundthread = (HANDLE) _beginthread(sound_thread_function, 0, NULL);
}

/**
 * Called on program shutdown to tidy up the sound thread
 */
void sound_thread_close(void)
{
        if (soundrunning)
        {
                sound_thread_wakeup();
                quitblitter=1;
                while (soundrunning)
                      sleep(1);
        }
}

void vidcwakeupthread(void)
{
        SetEvent(waitobject);
}

/**
 * A signal sent to the sound thread to let it
 * know that more data is available to be put in the
 * output buffer
 */
void sound_thread_wakeup(void)
{
        SetEvent(soundobject);
}

void vidcstartthread(void)
{
    HANDLE bltthread;

    waitobject=CreateEvent(NULL, FALSE, FALSE, NULL);
    bltthread=(HANDLE)_beginthread(vidcthreadrunner,0,NULL);
    SetThreadPriority(bltthread,THREAD_PRIORITY_TIME_CRITICAL);
    InitializeCriticalSectionAndSpinCount(&vidcmutex,0);
}

int vidctrymutex(void)
{
    return TryEnterCriticalSection(&vidcmutex);
}

void vidcreleasemutex(void)
{
    LeaveCriticalSection(&vidcmutex);
}


/**
 * Program start point, build the windows GUI, initialise the emulator.
 * Enter the program main loop, and tidy up when finished.
 */
int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
        static const char szClassName[] = "WindowsApp";
        MSG messages = {0};     /**< Here messages to the application are saved */
        WNDCLASSEX wincl;       /**< Data structure for the windowclass */

        hinstance=hThisInstance;

        /* The Window structure */
        wincl.hInstance = hThisInstance;
        wincl.lpszClassName = szClassName;
        wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
        wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
        wincl.cbSize = sizeof (WNDCLASSEX);

        /* Use custom icon and default mouse-pointer */
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
           "RPCEmu v" VERSION,      /* Title Text */
           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, /* overlapped window with no sizing frame */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           640 + (GetSystemMetrics(SM_CXFIXEDFRAME) * 2), /* The window width */
           480 + (GetSystemMetrics(SM_CYFIXEDFRAME) * 2) + GetSystemMetrics(SM_CYMENU) + GetSystemMetrics(SM_CYCAPTION), /* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           menu,                /* Menu handle */
           hThisInstance,       /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

        /* Make the window visible on the screen */
        ShowWindow (ghwnd, nFunsterStil);
        win_set_window(ghwnd);

        allegro_init();     /* allegro */

        /* Initialise the emulation and read the config file */
        if (startrpcemu())
           return -1;

        /* Initialise the podules */
        opendlls();

        /* Based on the contents of config file, dynamically update the Windows GUI items */
        if (config.cdromtype > 2) {
                WindowProcedure(ghwnd, WM_COMMAND, IDM_CDROM_DISABLED + config.cdromtype, 0);
        }
        CheckMenuItem(menu, IDM_CDROM_DISABLED + config.cdromtype, MF_CHECKED);
        
        CheckMenuItem(menu, IDM_STRETCH, config.stretchmode ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(menu, IDM_BLITOPT, config.skipblits   ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(menu, IDM_MOUSE_TWOBUTTON,
                      config.mousetwobutton ? MF_CHECKED : MF_UNCHECKED);
        
        if (config.mousehackon) {
                CheckMenuItem(menu, IDM_MOUSE_FOL, MF_CHECKED);
        } else {
                CheckMenuItem(menu, IDM_MOUSE_CAP, MF_CHECKED);
        }

        /* Return the mouse clipping to normal on program exit */
        atexit(releasemousecapture);

//        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        install_int_ex(vblupdate, BPS_TO_TIMER(config.refresh));
        drawscre=0;

        /* Program main loop */
        while (!quited)
        {
                /* Execute the emulation */
                execrpcemu();

                /* Update title with mips speed */
                if (updatemips)
                {
                        char title[128];

                        if (mousehack) {
                               sprintf(title, "RPCEmu v" VERSION " - MIPS: %.1f, AVG: %.1f",
                                       mips, mipstotal / mipscount);
                        } else {
                               sprintf(title, "RPCEmu v" VERSION " - MIPS: %.1f, AVG: %.1f - %s",
                                       mips, mipstotal / mipscount,
                                       (mousecapture) ?
                                           "Press CTRL-END to release mouse" :
                                           "Click to capture mouse");
                        }
                        SetWindowText(ghwnd, title);
                        updatemips=0;
                }

		if (handle_sigio) {
			handle_sigio = 0;
			sig_io(1);
		}

                /* Exit full screen? */
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && fullscreen)
                {
                        togglefullscreen(0);
                        mousecapture=0;
                }

                /* Release mouse from mouse capture mode? */
                if ((key[KEY_LCONTROL] || key[KEY_RCONTROL]) && key[KEY_END] && mousecapture && !config.mousehackon)
                {
                        ClipCursor(&oldclip);
                        mousecapture=0;
                        updatemips=1;
                }

                /* Handle Windows events */
                if (PeekMessage(&messages,NULL,0,0,PM_REMOVE))
                {
                        if (messages.message==WM_QUIT)
                        {
                                quited=1;
                        }
                        /* Translate virtual-key messages into character messages */
                        TranslateMessage(&messages);
                        /* Send message to WindowProcedure */
                        DispatchMessage(&messages);
                }
        }

        /* Program has exited. Tidy up */
        endrpcemu();
        
        /* The program return-value is 0 - The value that PostQuitMessage() gave */
        return messages.wParam;
}

/**
 * Creates an 'Open' dialog box to allow the user to choose a floppy image
 *
 * @param hwnd Handle to top level application window
 * @param drive Hardware floppy drive number (0 or 1)
 */
static void changedisc(HWND hwnd, int drive)
{
        char fn[512];
        char start[512];
        OPENFILENAME ofn;

        assert(hwnd);
        assert(drive == 0 || drive == 1);

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

	if (GetOpenFileName(&ofn)) {
		rpcemu_floppy_load(drive, fn);
	}
}

/**
 * Creates an 'Open' dialog box to allow the user to choose a CDROM image
 *
 * @param hwnd Handle to top level application window
 * @return 1 if a Filename was picked, 0 otherwise
 */
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

/**
 * Dialog procedure to handle evens on the configuration window
 *
 * @param hdlg    Handle to current program instance
 * @param message Event type this dialog has just received
 * @param wParam  message specific data
 * @param lParam  message specific data
 * @return
 */
static BOOL CALLBACK configdlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
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
                sprintf(s, "%d Hz", config.refresh);
                SendMessage(h,WM_SETTEXT,0,(LPARAM)s);

                /* Set Sound enabled */
                h = GetDlgItem(hdlg, CheckBox_Sound);
                SendMessage(h, BM_SETCHECK, config.soundenabled, 0);

                /* Set CPU model */
                /* The CPUModel list and Dialog items are in a different order */
                switch (config.model) {
                case CPUModel_ARM610:
                        cpu = 0; break;
                case CPUModel_ARM710:
                        cpu = 1; break;
                case CPUModel_SA110:
                        cpu = 2; break;
                case CPUModel_ARM7500:
                        cpu = 3; break;
                case CPUModel_ARM7500FE:
                        cpu = 4; break;
                case CPUModel_ARM810:
                        cpu = 5; break;
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
		switch (config.mem_size) {
		case 4:   h = GetDlgItem(hdlg, RadioButton_Mem_4); break;
		case 8:   h = GetDlgItem(hdlg, RadioButton_Mem_8); break;
		case 16:  h = GetDlgItem(hdlg, RadioButton_Mem_16); break;
		case 32:  h = GetDlgItem(hdlg, RadioButton_Mem_32); break;
		case 64:  h = GetDlgItem(hdlg, RadioButton_Mem_64); break;
		case 128: h = GetDlgItem(hdlg, RadioButton_Mem_128); break;
		}
		SendMessage(h, BM_SETCHECK, 1, 0);

                chosen_config.model        = config.model;
                chosen_config.vrammask     = config.vrammask;
                chosen_config.soundenabled = config.soundenabled;
                chosen_config.refresh      = config.refresh;
                return TRUE;

        case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                case IDOK:
		{
                        /* User has clicked on the OK button of the config dialog, 
                           apply their changes */
			int needs_reset = 0;

                        /* Sound turned off */
                        if (config.soundenabled && !chosen_config.soundenabled)
                        {
                                config.soundenabled = 0;
                                sound_pause();
                        }

                        /* Sound turned on */
                        if (chosen_config.soundenabled && !config.soundenabled)
                        {
                                config.soundenabled = 1;
                                sound_restart();
                        }

                        /* If an A7000 (ARM7500) or an A7000+ (ARM7500FE) it does not have vram */
                        if (config.model != chosen_config.model &&
                            (chosen_config.model == CPUModel_ARM7500 || chosen_config.model == CPUModel_ARM7500FE))
                        {
                                chosen_config.vrammask = 0;
                        }
                        
                        if (config.model != chosen_config.model ||
                            config.vrammask != chosen_config.vrammask ||
                            chngram)
                        {
                                needs_reset = 1;
                        }

                        if (chngram)
                        {
                                config.mem_size = chosen_config.mem_size;
                        }
                        config.model = chosen_config.model;
                        config.vrammask = chosen_config.vrammask;
                        config.refresh = chosen_config.refresh;

			/* Reset the machine after the config variables have been set to their
			   new values */
			if (needs_reset) {
				resetrpc();
			}
                        install_int_ex(vblupdate, BPS_TO_TIMER(config.refresh));
                        EndDialog(hdlg,0);
                        return TRUE;
		}

                case IDCANCEL:
                        EndDialog(hdlg,0);
                        return TRUE;

                /* VRAM None */
                case RadioButton_VRAM_0:
                        h = GetDlgItem(hdlg, RadioButton_VRAM_0);
                        SendMessage(h,BM_SETCHECK,1,0);
                        h = GetDlgItem(hdlg, RadioButton_VRAM_2);
                        SendMessage(h,BM_SETCHECK,0,0);
                        chosen_config.vrammask = 0;
                        return TRUE;

                /* VRAM 2MB */
                case RadioButton_VRAM_2:
                        h = GetDlgItem(hdlg, RadioButton_VRAM_0);
                        SendMessage(h,BM_SETCHECK,0,0);
                        h = GetDlgItem(hdlg, RadioButton_VRAM_2);
                        SendMessage(h,BM_SETCHECK,1,0);
                        chosen_config.vrammask = 0x7FFFFF;
                        return TRUE;
                        
                /* CPU Type radio buttons */
                case RadioButton_ARM610:
                case RadioButton_ARM710:
                case RadioButton_SA110:
                case RadioButton_ARM7500:
                case RadioButton_ARM7500FE:
                case RadioButton_ARM810:
                        /* The model enum and the dialog IDs are in different orders */

                        /* Clear previous CPU model choice */
                        switch (chosen_config.model) {
                        case CPUModel_ARM610:
                                cpu = 0; break;
                        case CPUModel_ARM710:
                                cpu = 1; break;
                        case CPUModel_SA110:
                                cpu = 2; break;
                        case CPUModel_ARM7500:
                                cpu = 3; break;
                        case CPUModel_ARM7500FE:
                                cpu = 4; break;
                        case CPUModel_ARM810:
                                cpu = 5; break;
                        default:
                                fprintf(stderr, "configdlgproc(): unknown CPU model %d\n", config.model);
                                exit(EXIT_FAILURE);
                        }

                        h = GetDlgItem(hdlg, RadioButton_ARM610 + cpu);
                        SendMessage(h, BM_SETCHECK, 0, 0);

                        /* Set new CPU model */
                        switch (LOWORD(wParam) - RadioButton_ARM610) {
                        case 0:
                                chosen_config.model = CPUModel_ARM610; break;
                        case 1:
                                chosen_config.model = CPUModel_ARM710; break;
                        case 2:
                                chosen_config.model = CPUModel_SA110; break;
                        case 3:
                                chosen_config.model = CPUModel_ARM7500; break;
                        case 4:
                                chosen_config.model = CPUModel_ARM7500FE; break;
                        case 5:
                                chosen_config.model = CPUModel_ARM810; break;
                        default:
                                fatal("configdlgproc(): unknown dialog item for CPUModel %d\n",
                                      LOWORD(wParam) - RadioButton_ARM610);
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
			switch (LOWORD(wParam)) {
			case RadioButton_Mem_4:   chosen_config.mem_size = 4; break;
			case RadioButton_Mem_8:   chosen_config.mem_size = 8; break;
			case RadioButton_Mem_16:  chosen_config.mem_size = 16; break;
			case RadioButton_Mem_32:  chosen_config.mem_size = 32; break;
			case RadioButton_Mem_64:  chosen_config.mem_size = 64; break;
			case RadioButton_Mem_128: chosen_config.mem_size = 128; break;
			}
                        for (c = RadioButton_Mem_4; c <= RadioButton_Mem_128; c++)
                        {
                                h=GetDlgItem(hdlg,c);
                                SendMessage(h,BM_SETCHECK,0,0);
                        }
                        h=GetDlgItem(hdlg,LOWORD(wParam));
                        SendMessage(h,BM_SETCHECK,1,0);
                        if (chosen_config.mem_size != config.mem_size) {
                                chngram = 1;
                        } else {
                                chngram = 0;
                        }
                        return TRUE;
                        
                /* Sound */
                case CheckBox_Sound:
                        chosen_config.soundenabled ^= 1;
                        h=GetDlgItem(hdlg,LOWORD(wParam));
                        SendMessage(h, BM_SETCHECK, chosen_config.soundenabled, 0);
                        return TRUE;
                }
                break;

        case WM_HSCROLL:
                h = GetDlgItem(hdlg, Slider_Refresh);
                c=SendMessage(h,TBM_GETPOS,0,0);
                h = GetDlgItem(hdlg, Text_Refresh);
                sprintf(s, "%d Hz", c * 5);
                SendMessage(h,WM_SETTEXT,0,(LPARAM)s);
                chosen_config.refresh = c * 5;
                break;

        }
        return FALSE;
}

/**
 * Dialog procedure to handle events on the networking window
 *
 * @param hdlg    Handle to current program instance
 * @param message Event type this dialog has just received
 * @param wParam  message specific data
 * @param lParam  message specific data
 * @return
 */
static BOOL CALLBACK
networkdlgproc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	char default_bridgename[] = "rpcemu";

	switch (message) {
	case WM_INITDIALOG:
		/* Network Type */
		if (config.network_type == NetworkType_EthernetBridging) {
			SendMessage(GetDlgItem(hdlg, RadioButton_EthernetBridging), BM_SETCHECK, 1, 0);

			/* Make sure Bridgename is visible */
			EnableWindow(GetDlgItem(hdlg, Edit_BridgeName), 1);
			EnableWindow(GetDlgItem(hdlg, Text_BridgeName), 1);
		} else {
			SendMessage(GetDlgItem(hdlg, RadioButton_Off), BM_SETCHECK, 1, 0);

			/* Disable bridgename */
			EnableWindow(GetDlgItem(hdlg, Edit_BridgeName), 0);
			EnableWindow(GetDlgItem(hdlg, Text_BridgeName), 0);
		}

		/* Bridge name */
		SendMessage(GetDlgItem(hdlg, Edit_BridgeName), WM_SETTEXT, 0,
		            config.bridgename ? (LPARAM) config.bridgename : (LPARAM) default_bridgename);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		{
			NetworkType selected_network_type = NetworkType_Off;
			char gui_bridgename[50] = "";

			/* User has clicked on the OK button of the config dialog,
			   apply their changes */

			if (SendMessage(GetDlgItem(hdlg, RadioButton_Off), BM_GETCHECK, 0, 0) == BST_CHECKED) {
				selected_network_type = NetworkType_Off;
			} else if (SendMessage(GetDlgItem(hdlg, RadioButton_EthernetBridging), BM_GETCHECK, 0, 0) == BST_CHECKED) {
				selected_network_type = NetworkType_EthernetBridging;
			} else {
				rpclog("Unknown network type returned from GUI, defaulting to off");
				selected_network_type = NetworkType_Off;
			}

			GetWindowText(GetDlgItem(hdlg, Edit_BridgeName), (LPSTR) &gui_bridgename, 50);

			/* Pass on the values to the core, to see if we need to reboot */
			if (network_config_changed(selected_network_type, gui_bridgename, NULL)) {
				resetrpc();
			}

			EndDialog(hdlg, 0);
			return TRUE;
		}

		case IDCANCEL:
			EndDialog(hdlg, 0);
			return TRUE;

		case RadioButton_EthernetBridging:
			SendMessage(GetDlgItem(hdlg, RadioButton_Off), BM_SETCHECK, 0, 0);
			SendMessage(GetDlgItem(hdlg, RadioButton_EthernetBridging), BM_SETCHECK, 1, 0);

			/* Make sure Bridgename is visible */
			EnableWindow(GetDlgItem(hdlg, Edit_BridgeName), 1);
			EnableWindow(GetDlgItem(hdlg, Text_BridgeName), 1);
			return TRUE;

		case RadioButton_Off:
			SendMessage(GetDlgItem(hdlg, RadioButton_EthernetBridging), BM_SETCHECK, 0, 0);
			SendMessage(GetDlgItem(hdlg, RadioButton_Off), BM_SETCHECK, 1, 0);

			/* Disable bridgename */
			EnableWindow(GetDlgItem(hdlg, Edit_BridgeName), 0);
			EnableWindow(GetDlgItem(hdlg, Text_BridgeName), 0);
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Window procedure to handle events on the main program window
 *
 * @param hdlg    Handle to current program instance
 * @param message Event type this window has just received
 * @param wParam  message specific data
 * @param lParam  message specific data
 * @return
 */
static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        HMENU hmenu;
        switch (message)                  /* handle the messages */
        {
        case WM_CREATE:
                strcpy(config.isoname, "");
                return 0;

        case WM_COMMAND:
                hmenu=GetMenu(hwnd);
                switch (LOWORD(wParam))
                {
                case IDM_FILE_RESET:
                        resetrpc();
                        return 0;

                case IDM_FILE_EXIT:
                        closevideo();
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

                case IDM_NETWORKING:
                        DialogBox(hinstance, TEXT("NetworkDlg"), ghwnd, networkdlgproc);
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
                                if (MessageBox(ghwnd, "This will reset RPCEmu!\nOkay to continue?", "RPCEmu", MB_OKCANCEL) == IDOK)
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
                                if (MessageBox(ghwnd, "This will reset RPCEmu!\nOkay to continue?", "RPCEmu", MB_OKCANCEL) == IDOK)
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
                                        if (MessageBox(ghwnd, "This will reset RPCEmu!\nOkay to continue?", "RPCEmu", MB_OKCANCEL) == IDOK)
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

                case IDM_MOUSE_TWOBUTTON:
                        config.mousetwobutton ^= 1;
                        CheckMenuItem(hmenu, IDM_MOUSE_TWOBUTTON,
                                      config.mousetwobutton ? MF_CHECKED : MF_UNCHECKED);
                        return 0;
                }

                if (LOWORD(wParam)>=IDM_CDROM_REAL && LOWORD(wParam)<(IDM_CDROM_REAL+100))
                {
                        if (!config.cdromenabled)
                        {
                                if (MessageBox(ghwnd, "This will reset RPCEmu!\nOkay to continue?", "RPCEmu", MB_OKCANCEL) == IDOK)
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
                PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
                break;

        case WM_SETFOCUS:
                resetbuffer();
                break;

        case WM_KILLFOCUS:
                if (mousecapture)
                {
                        ClipCursor(&oldclip);
                        mousecapture=0;
                }
                break;

        case WM_LBUTTONUP:
                if (!mousecapture && !fullscreen && !config.mousehackon)
                {
                        RECT arcclip;

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
