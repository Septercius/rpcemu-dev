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

/* RPCemu networking */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>

#include "rpcemu.h"
#include "mem.h"
#include "podules.h"
#include "network.h"

int
network_plt_init(void)
{
    // Do nothing on a Mac, as TUN/TAP is not supported.
    return 0;
}

/**
 * Shutdown any running network components.
 *
 * Called on program shutdown and program reset after
 * configuration has changed.
 */
void
network_plt_reset(void)
{
    // Do nothing on a Mac, as TUN/TAP is not supported.
}

uint32_t
network_plt_rx(uint32_t errbuf, uint32_t mbuf, uint32_t rxhdr, uint32_t *dataavail)
{
    NOT_USED(errbuf);
    NOT_USED(mbuf);
    NOT_USED(rxhdr);
    NOT_USED(dataavail);
    
    // Do nothing on a Mac, as TUN/TAP is not supported.
    return 0;
}

uint32_t
network_plt_tx(uint32_t errbuf, uint32_t mbufs, uint32_t dest, uint32_t src, uint32_t frametype)
{
    NOT_USED(errbuf);
    NOT_USED(mbufs);
    NOT_USED(dest);
    NOT_USED(src);
    NOT_USED(frametype);

    // Do nothing on a Mac, as TUN/TAP is not supported.
    return 0;
}

void
network_plt_setirqstatus(uint32_t address)
{
    NOT_USED(address);
    
    // Do nothing on a Mac, as TUN/TAP is not supported.
}
