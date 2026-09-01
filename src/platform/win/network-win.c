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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "rpcemu.h"
#include "mem.h"
#include "network.h"
#include "podules.h"
#include "tap.h"

#define HEADERLEN	14

// The opened tunnel device
static void *tap_handle = NULL;

// Pointer to a word in RMA, used as the IRQ status register
static uint32_t irqstatus;

// Max packet is 1500 bytes plus headers
static uint8_t buffer[1522];

/**
 * Transmit data to the network
 *
 * @param errbuf    Address of buffer to return error string
 * @param mbufs     Address of mbuf chain containing data to send
 * @param dest      Address of destination MAC address
 * @param src       Address of source MAC address, or 0 to use default
 * @param frametype EtherType of frame
 *
 * @return errbuf on error, else zero
 */
uint32_t
network_plt_tx(uint32_t errbuf, uint32_t mbufs, uint32_t dest, uint32_t src, uint32_t frametype)
{
	uint8_t *buf = buffer;
	struct ro_mbuf_part txb;
	size_t packet_length;
	int ret;

	if (tap_handle == NULL) {
		strcpyfromhost(errbuf, "RPCEmu: Networking not available");
		return errbuf;
	}

	/* Ethernet packet is
	   6 bytes destination MAC address
	   6 bytes source MAC address
	   2 bytes frame type (Ethernet II) or length (IEEE 802.3)
	   up to 1500 bytes payload
	*/

	memcpytohost(buf, dest, 6);
	buf += 6;

	if (src != 0) {
		memcpytohost(buf, src, 6);
	} else {
		/* Make up a MAC address. As this is only going on the TAP
		   device and not a real ethernet then it doesn't need to
		   be unique, just different to the MAC on the other end
		   of the tunnel. */
		memcpy(buf, network_hwaddr, 6);
	}
	buf += 6;

	*buf++ = (uint8_t) (frametype >> 8);
	*buf++ = (uint8_t) frametype;

	packet_length = HEADERLEN;

	// Copy the mbuf chain as the payload
	while (mbufs != 0) {
		memcpytohost(&txb, mbufs, sizeof(txb));
		packet_length += txb.m_len;
		if (packet_length > sizeof(buffer)) {
			strcpyfromhost(errbuf, "RPCEmu: Packet too large to send");
			return errbuf;
		}
		memcpytohost(buf, mbufs + txb.m_off, txb.m_len);
		buf += txb.m_len;
		mbufs = txb.m_next;
	}

	ret = tap_send(tap_handle, buffer, packet_length);
	if (ret == -1) {
		strcpyfromhost(errbuf, strerror(errno));
		return errbuf;
	}

	return 0;
}

/**
 * Receive data from the network
 *
 * @param errbuf     Address of buffer to return error string
 * @param mbuf       Address of mbuf to hold received payload
 * @param rxhdr      Address of mbuf to hold received header
 * @param data_avail Address of flag to return indication of data available
 *
 * @return errbuf on error, else zero
 */
uint32_t
network_plt_rx(uint32_t errbuf, uint32_t mbuf, uint32_t rxhdr, uint32_t *data_avail)
{
	struct ro_mbuf_part rxb;
	struct rx_hdr hdr;
	int packet_length;

	*data_avail = 0;

	if (tap_handle == NULL) {
		// Networking not available
		return errbuf;
	}

	memset(&hdr, 0, sizeof(hdr));

	packet_length = tap_receive(tap_handle, buffer, sizeof(buffer));

	if (mbuf != 0 && packet_length > HEADERLEN) {
		const uint8_t *payload = buffer + HEADERLEN;

		// Fill in received header structure
		memcpy(hdr.rx_dst_addr, buffer + 0, 6);
		memcpy(hdr.rx_src_addr, buffer + 6, 6);
		hdr.rx_frame_type = (buffer[12] << 8) | buffer[13];
		hdr.rx_error_level = 0;
		memcpyfromhost(rxhdr, &hdr, sizeof(hdr));

		packet_length -= HEADERLEN;

		memcpytohost(&rxb, mbuf, sizeof(rxb));

		if ((size_t) packet_length > rxb.m_inilen) {
			// Mbuf too small for received packet
			return errbuf;
		}

		// Copy payload in to the mbuf
		rxb.m_off = rxb.m_inioff;
		memcpyfromhost(mbuf + rxb.m_off, payload, packet_length);
		rxb.m_len = packet_length;
		memcpyfromhost(mbuf, &rxb, sizeof(rxb));

		*data_avail = 1;
	}

	return 0;
}

void
sig_io(int sig)
{
	NOT_USED(sig);

	if (irqstatus != 0) {
		network_irq_raise();
	}
}

/**
 * @param address
 */
void
network_plt_setirqstatus(uint32_t address)
{
	irqstatus = address;
}

int
network_plt_init(void)
{ 
	if (config.network_type == NetworkType_IPTunnelling) {
		error("IP Tunnelling networking is not supported on Windows");
		return 0;
	}

	if (config.bridgename == NULL) {
		error("Bridge name not configured");
		return 0;
	}

	if (config.macaddress) {
		// Parse supplied MAC address
		if (!network_macaddress_parse(config.macaddress, network_hwaddr)) {
			error("Unable to parse '%s' as a MAC address", config.macaddress);
			return 0;
		}
	} else {
		network_hwaddr[0] = 0x06;
		network_hwaddr[1] = 0x02;
		network_hwaddr[2] = 0x03;
		network_hwaddr[3] = 0x04;
		network_hwaddr[4] = 0x05;
		network_hwaddr[5] = 0x06;
	}

	tap_handle = tap_init(config.bridgename);
	if (tap_handle == NULL) {
		return 0;
	} else {
		return 1;
	}
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
	if (tap_handle != NULL) {
		tap_cleanup(tap_handle);
		tap_handle = NULL;
	}
}
