#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "rpcemu.h"
#include "mem.h"
#include "network.h"
#include "network-nat.h"
#include "podules.h"

#include "slirp/libslirp.h"

#define PIPE_PATH	"/tmp/rpcemu_net_nat"

#define HEADERLEN	14

static struct {
	Slirp		*slirp;

	uint32_t	irq_status;	///< Address of a word in RAM, used as the IRQ status register

	uint8_t		buffer[2048];

	size_t		buffer_len;

	FILE		*capture;	///< Handle for debug capture file, or NULL if not in use
} nat;

static void
write16(FILE *f, uint16_t x)
{
	(void) fwrite(&x, sizeof(uint16_t), 1, f);
}

static void
write32(FILE *f, uint32_t x)
{
	(void) fwrite(&x, sizeof(uint32_t), 1, f);
}

/**
 */
static void
write_global_header(FILE *f)
{
	// The magic number is adaptive-endian.
	// It's written in the host's native order, and the reader is
	// expected to interpret all data fields accordingly.
	write32(f, 0xa1b2c3d4); // magic number

	write16(f, 2); // version_major
	write16(f, 4); // version_minor
	write32(f, 0); // thiszone
	write32(f, 0); // sigfigs
	write32(f, 65535); // snaplen
	write32(f, 1); // data link type - 1 for Ethernet
	(void) fflush(f);
}

/**
 * Write a packet to the capture file.
 *
 * @param f
 * @param data
 * @param data_len
 */
static void
write_packet(FILE *f, const void *data, size_t data_len)
{
	struct timeval tv;

	// Check that the File was opened successfully
	if (f == NULL) {
		return;
	}

	if (gettimeofday(&tv, NULL) != 0) {
		return;
	}

	write32(f, (uint32_t) tv.tv_sec); // timestamp seconds
	write32(f, (uint32_t) tv.tv_usec); // timestamp microseconds
	write32(f, (uint32_t) data_len); // number of octets of packet saved in file
	write32(f, (uint32_t) data_len); // actual length of packet
	(void) fwrite(data, 1, data_len, f); // packet data
	(void) fflush(f);
}

/**
 */
void
slirp_output(void *opaque, const uint8_t *pkt, int pkt_len)
{
	NOT_USED(opaque);

	// Write to capture file for debug
	write_packet(nat.capture, pkt, pkt_len);

	if (nat.irq_status == 0 || network_poduleinfo == NULL) {
		// Not set-up to generate IRQ
		return;
	}

	memcpy(nat.buffer, pkt, pkt_len);
	nat.buffer_len = pkt_len;

	mem_write8(nat.irq_status, 1);
	network_poduleinfo->irq = 1;
	rethinkpoduleints();
}

/**
 */
int
slirp_can_output(void *opaque)
{
	NOT_USED(opaque);

	return (nat.buffer_len == 0);
}

/**
 */
static void
network_nat_init_mac_address(void)
{
	if (config.macaddress != NULL) {
		// Parse supplied MAC address
		if (network_macaddress_parse(config.macaddress, network_hwaddr)) {
			return;
		}
		error("Unable to parse '%s' as a MAC address", config.macaddress);
	}

	// Generate MAC address
	network_hwaddr[0] = 0x06;
	network_hwaddr[1] = 0x02;
	network_hwaddr[2] = 0x03;
	network_hwaddr[3] = 0x04;
	network_hwaddr[4] = 0x05;
	network_hwaddr[5] = 0x06;
}

/**
 */
static void
network_nat_open(void)
{
	struct in_addr host;
	struct in_addr mask;
	struct in_addr net_addr;
	struct in_addr dns;
	struct in_addr dhcp;
	const int restricted = 0;
	const char *vhostname = NULL;
	const char *bootfile = NULL;

	host.s_addr = htonl(0x0a0a0a02); // 10.10.10.2
	mask.s_addr = htonl(0xffffff00); // 255.255.255.0
	net_addr.s_addr = htonl(0x0a0a0a00); // 10.10.10.0
	dns.s_addr = htonl(0x0a0a0a03); // 10.10.10.3
	dhcp.s_addr = htonl(0x0a0a0a0a); // 10.10.10.10

	// Initialise, but only once
	if (nat.slirp == NULL) {
		nat.slirp = slirp_init(restricted,
		    net_addr, mask, host, vhostname, "", bootfile, dhcp, dns, NULL);

		// TODO log NAT details
	}
}

int
network_nat_init(void)
{
	//nat.buffer_len = 0;

	// MAC address
	network_nat_init_mac_address();

	network_nat_open();

	// Open capture file if requested
	if (config.network_capture != NULL) {
		if ((nat.capture = fopen(config.network_capture, "wb")) != NULL) {
			// Write header
			write_global_header(nat.capture);

			rpclog("Networking: capturing to \"%s\"\n", config.network_capture);
		}
	}

	return 1;
}

void
network_nat_reset(void)
{
	nat.irq_status = 0;
	nat.buffer_len = 0;
}

void
network_nat_poll(void)
{
	fd_set rfds, wfds, efds;
	int fd_max, ret;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);
	fd_max = -1;

	slirp_select_fill(nat.slirp, &fd_max, &rfds, &wfds, &efds);
	if (fd_max == -1) {
		return;
	}

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	ret = select(fd_max + 1, &rfds, &wfds, &efds, &tv);
	if (ret < 0) {
		return;
	}

	slirp_select_poll(nat.slirp, &rfds, &wfds, &efds, ret <= 0);
}

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
network_nat_tx(uint32_t errbuf, uint32_t mbufs, uint32_t dest, uint32_t src, uint32_t frametype)
{
	uint8_t *buf = nat.buffer;
	struct mbuf txb;
	uint32_t packet_length;

	memcpytohost(buf, dest, 6);
	buf += 6;

	if (src != 0) {
		memcpytohost(buf, src, 6);
	} else {
		memcpy(buf, network_hwaddr, 6);
	}
	buf += 6;

	*buf++ = (uint8_t) (frametype >> 8);
	*buf++ = (uint8_t) frametype;

	packet_length = HEADERLEN;
	while (mbufs != 0) {
		memcpytohost(&txb, mbufs, sizeof(struct mbuf));
		packet_length += txb.m_len;
		if (packet_length > sizeof(nat.buffer)) {
			strcpyfromhost(errbuf, "RPCEmu: Packet too large to send");
			return errbuf;
		}
		memcpytohost(buf, mbufs + txb.m_off, txb.m_len);
		buf += txb.m_len;
		mbufs = txb.m_next;
	}

	// Write to capture file for debug
	write_packet(nat.capture, nat.buffer, packet_length);

	slirp_input(nat.slirp, nat.buffer, packet_length);

	return 0;
}

/**
 * Receive data from the network
 *
 * @param errbuf
 * @param mbuf
 * @param rxhdr
 * @param data_avail
 *
 * @return
 */
uint32_t
network_nat_rx(uint32_t errbuf, uint32_t mbuf, uint32_t rxhdr, uint32_t *data_avail)
{
	struct mbuf rxb;
	struct rx_hdr hdr;
	size_t packet_length;

	*data_avail = 0;

	if (nat.buffer_len == 0) {
		return errbuf;
	}

	memset(&hdr, 0, sizeof(hdr));

	packet_length = nat.buffer_len;

	if (mbuf != 0 && packet_length > HEADERLEN) {
		const uint8_t *payload = nat.buffer + HEADERLEN;

		// Fill in received header structure
		memcpy(hdr.rx_dst_addr, nat.buffer + 0, 6);
		memcpy(hdr.rx_src_addr, nat.buffer + 6, 6);
		hdr.rx_frame_type = (nat.buffer[12] << 8) | nat.buffer[13];
		hdr.rx_error_level = 0;
		memcpyfromhost(rxhdr, &hdr, sizeof(hdr));

		packet_length -= HEADERLEN;

		memcpytohost(&rxb, mbuf, sizeof(rxb));

		if (packet_length > rxb.m_inilen) {
			fprintf(stderr, "\tmbuff too small for received packet\n");
			strcpyfromhost(errbuf, "RPCEmu: Mbuf too small for received packet");
			return errbuf;
		}

		// Copy payload in to the mbuf
		rxb.m_off = rxb.m_inioff;
		memcpyfromhost(mbuf + rxb.m_off, payload, packet_length);
		rxb.m_len = packet_length;
		memcpyfromhost(mbuf, &rxb, sizeof(rxb));

		*data_avail = 1;

		nat.buffer_len = 0;
	}

	return 0;
}

/**
 * @param address
 */
void
network_nat_setirqstatus(uint32_t address)
{
	nat.irq_status = address;
}
