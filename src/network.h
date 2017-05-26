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

#ifndef NETWORK_H
#define NETWORK_H

#include "rpcemu.h"
#include "podules.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* RPCemu networking */

// r0: Reason code in r0
// r1: Pointer to buffer for any error string
// r2-r5: Reason code dependent
// Returns 0 in r0 on success, non 0 otherwise and error buffer filled in
void network_swi(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                 uint32_t r4, uint32_t r5, uint32_t *retr0, uint32_t *retr1);

void network_init(void);
void network_reset(void);

/* Functions shared between each platform, in network.c */
void memcpytohost(void *dest, uint32_t src, uint32_t len);
void memcpyfromhost(uint32_t dest, const void *source, uint32_t len);
void strcpyfromhost(uint32_t dest, const char *source);

int network_config_changed(NetworkType networktype, const char *bridgename,
                           const char *ipaddress);
int network_macaddress_parse(const char *macaddress, uint8_t hwaddr[6]);

/* Functions provided by each host platform's network code */
void network_plt_reset(void);
int network_plt_init(void);
uint32_t network_plt_tx(uint32_t errbuf, uint32_t mbufs, uint32_t dest, uint32_t src, uint32_t frametype);
uint32_t network_plt_rx(uint32_t errbuf, uint32_t mbuf, uint32_t rxhdr, uint32_t *dataavail);
void network_plt_setirqstatus(uint32_t address);

/* Structures and variables shared between each host platform's network code */
extern podule *network_poduleinfo;
extern unsigned char network_hwaddr[6];

/* Structures to represent the RISC OS view of things */
struct pkthdr {
    uint32_t len;
    uint32_t rcvif;
};

struct mbuf {
    uint32_t m_next;
    uint32_t m_list;
    uint32_t m_off;
    uint32_t m_len;
    uint32_t m_inioff;
    uint32_t m_inilen;
    uint8_t m_type;
    uint8_t m_sys1;
    uint8_t m_sys2;
    uint8_t m_flags;
    struct pkthdr m_pkthdr;
};

struct rx_hdr {
    uint32_t rx_ptr;
    uint32_t rx_tag;
    uint8_t rx_src_addr[6];
    uint8_t _spad[2];
    uint8_t rx_dst_addr[6];
    uint8_t _dpad[2];
    uint32_t rx_frame_type;
    uint32_t rx_error_level;
    uint32_t rx_cksum;
};

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* NETWORK_H */
