/* RPCemu networking */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "rpcemu.h"
#include "mem.h"
#include "podules.h"
#include "network.h"
#include "tap.h"

/* The opened tunnel device */
static void *tap_handle = NULL;

/* Max packet is 1500 bytes plus headers */
static unsigned char buffer[1522];

#define HEADERLEN 14

/* Transmit data
   errbuf - pointer to buffer to return error string
   mbufs - pointer to mbuf chain containing data to send
   dest - pointer to detination MAC address
   src - pointer to source MAC address, or 0 to use default

   returns errbuf on error, else zero
*/
uint32_t
network_plt_tx(uint32_t errbuf, uint32_t mbufs, uint32_t dest, uint32_t src, uint32_t frametype)
{
    unsigned char *buf = buffer;
    struct mbuf txb;
    int packetlength;
    int ret;
    int i;

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

    if (src) {
        memcpytohost(buf, src, 6);
    } else {
        /* Make up a MAC address. As this is only going on the TAP
           device and not a real ethernet then it doesn't need to
           be unique, just different to the MAC on the other end
           of the tunnel. */
        for (i = 0; i < 6; i++) {
            buf[i] = network_hwaddr[i];
        }
    }
    buf += 6;

    *buf++ = (frametype>>8) & 0xFF;
    *buf++ = frametype & 0xFF;

#if defined RPCLOG
    rpclog("===== SEND HEADER =====\n");
    rpclog("dst_addr: %02x:%02x:%02x:%02x:%02x:%02x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
    rpclog("src_addr: %02x:%02x:%02x:%02x:%02x:%02x\n", buffer[6], buffer[7], buffer[8], buffer[9], buffer[10], buffer[11]);
    rpclog("frame_type: %d\n", frametype);
#endif

    packetlength = HEADERLEN;


    /* Copy the mbuf chain as the payload */
    while (mbufs) {
        memcpytohost(&txb, mbufs, sizeof(struct mbuf));
        packetlength += txb.m_len;
        if (packetlength > sizeof(buffer)) {
            strcpyfromhost(errbuf, "RPCEmu: Packet too large to send");
            return errbuf;
        }
        memcpytohost(buf, mbufs + txb.m_off, txb.m_len);
        buf += txb.m_len;
        mbufs = txb.m_next;
    }

#if defined RPCLOG
    rpclog("send %d bytes\n", packetlength);

    if (packetlength > 0) {
	char dump[2048 * 3];
	int offset = 0;
	int i;
	for (i = 0; i < packetlength; i++) {
	    offset += sprintf(dump + offset, "%02x ", buffer[i]);
	}
	rpclog("data: %s\n", dump);
    }
#endif

    ret = tap_send(tap_handle, buffer, packetlength);
    if (ret == -1) {
        strcpyfromhost(errbuf, strerror(errno));
        return errbuf;
    }

    return 0;
}


uint32_t
network_plt_rx(uint32_t errbuf, uint32_t mbuf, uint32_t rxhdr, uint32_t *dataavail)
{
    struct mbuf rxb;
    struct rx_hdr hdr;
    int packetlength;

    *dataavail = 0;

    if (tap_handle == NULL) {
        strcpyfromhost(errbuf, "RPCEmu: Networking not available");
        return errbuf;
    }

    memset(&hdr, 0, sizeof(hdr));

    packetlength = tap_receive(tap_handle, buffer, sizeof(buffer));

#if defined RPCLOG
    rpclog("received %d bytes\n", packetlength);

    if (packetlength > 0) {
	char dump[2048 * 3];
	int offset = 0;
	int i;
	for (i = 0; i < packetlength; i++) {
	    offset += sprintf(dump + offset, "%02x ", buffer[i]);
	}
	rpclog("data: %s\n", dump);
    }
#endif

    if (mbuf && packetlength > HEADERLEN) {
        unsigned char *payload = buffer + HEADERLEN;

        /* Fill in recieved header structure */
        memcpy(hdr.rx_dst_addr, buffer + 0, 6);
        memcpy(hdr.rx_src_addr, buffer + 6, 6);
        hdr.rx_frame_type = (buffer[12] << 8) | buffer[13];
        hdr.rx_error_level = 0;
        memcpyfromhost(rxhdr, &hdr, sizeof(hdr));

#if defined RPCLOG
	rpclog("===== HEADER =====\n");
	rpclog("dst_addr: %02x:%02x:%02x:%02x:%02x:%02x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
	rpclog("src_addr: %02x:%02x:%02x:%02x:%02x:%02x\n", buffer[6], buffer[7], buffer[8], buffer[9], buffer[10], buffer[11]);
	rpclog("frame_type: %d\n", hdr.rx_frame_type);
#endif

        packetlength -= HEADERLEN;
        
        memcpytohost(&rxb, mbuf, sizeof(rxb));

        if (packetlength > rxb.m_inilen) {
            strcpyfromhost(errbuf, "RPCEmu: Mbuf too small for received packet");
            return errbuf;
        } else {
            /* Copy payload in to the mbuf */
            rxb.m_off = rxb.m_inioff;
            memcpyfromhost(mbuf + rxb.m_off, payload, packetlength);
            rxb.m_len = packetlength;
            memcpyfromhost(mbuf, &rxb, sizeof(rxb));

            *dataavail = 1;
        }

#if defined RPCLOG
        rpclog("============\n");
#endif

    }
    return 0;
}

// Pointer to a word in RMA, used as the IRQ status register
static uint32_t irqstatus = 0;

void
network_plt_setirqstatus(uint32_t address)
{
    irqstatus = address;
}

void sig_io(int sig)
{
    if (irqstatus) {
        writememb(irqstatus, 1);
        if (network_poduleinfo) {
            network_poduleinfo->irq = 1;
        }
        rethinkpoduleints();
    }
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
        /* Parse supplied MAC address */
        sscanf(config.macaddress, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &network_hwaddr[0], &network_hwaddr[1], &network_hwaddr[2],
               &network_hwaddr[3], &network_hwaddr[4], &network_hwaddr[5]);
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
	if (tap_handle) {
		tap_cleanup(tap_handle);
		tap_handle = NULL;
	}
}
