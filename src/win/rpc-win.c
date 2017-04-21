/*
  RPCEmu - An Acorn system emulator

  Copyright (C) 2005-2010 Sarah Walker

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Windows specific stuff */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#undef UNICODE
#include <windows.h>

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
#include "cdrom-iso.h"
#include "cdrom-ioctl.h"
#include "network.h"

int handle_sigio; /**< bool to indicate new network data is received */

/**
 * Return disk space information about a file system.
 *
 * @param path Pathname of object within file system
 * @param d    Pointer to disk_info structure that will be filled in
 * @return     On success 1 is returned, on error 0 is returned
 */
int
path_disk_info(const char *path, disk_info *d)
{
	ULARGE_INTEGER free, total;

	assert(path != NULL);
	assert(d != NULL);

	if (GetDiskFreeSpaceEx(path, &free, &total, NULL) == 0) {
		return 0;
	}

	d->size = (uint64_t) total.QuadPart;
	d->free = (uint64_t) free.QuadPart;

	return 1;
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
