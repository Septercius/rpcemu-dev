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

/*
 network.c - code shared between each host platforms networking support
 */
#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "rpcemu.h"
#include "mem.h"
#include "network.h"
#include "network-nat.h"
#include "podules.h"

/* Variables for supporting a podule header data */
static uint8_t *romdata = NULL; /**< Podule header data and the like */
static uint32_t poduleromsize = 0;
static const char description[] = "RPCEmu Ethernet";
static uint32_t chunkbase;
static uint32_t filebase;


podule *network_poduleinfo = NULL;

unsigned char network_hwaddr[6]; /**< MAC Hardware address */


/**
 *
 *
 * @param type
 * @param filebase
 * @param size
 */
static void
makechunk(uint8_t type, uint32_t filebase, uint32_t size)
{
	romdata[chunkbase++] = type;
	romdata[chunkbase++] = (uint8_t) size;
	romdata[chunkbase++] = (uint8_t) (size >> 8);
	romdata[chunkbase++] = (uint8_t) (size >> 16);

	romdata[chunkbase++] = (uint8_t) filebase;
	romdata[chunkbase++] = (uint8_t) (filebase >> 8);
	romdata[chunkbase++] = (uint8_t) (filebase >> 16);
	romdata[chunkbase++] = (uint8_t) (filebase >> 24);
}

/**
 * Create the network podule ROM consisting of a description string and a
 * driver module
 */
static void
network_rom_init(void)
{
	FILE *f;
	size_t module_file_size = 0;

	// Build podule header
	chunkbase = 0x10;
	filebase = chunkbase + (8 * 2) + 4; // required size for two entries
	poduleromsize = filebase + ((sizeof(description) + 3) & ~3u); // Word align description string

	// Add on size for driver module if it can be opened successfully
	f = fopen("netroms/EtherRPCEm,ffa", "rb");
	if (f != NULL) {
		long len;

		fseek(f, 0, SEEK_END);
		len = ftell(f);
		if (len > 0 && len <= 128 * 1024) {
			module_file_size = (size_t) len;
			poduleromsize += ((uint32_t) len + 3) & ~3u; // Word-aligned size
			rewind(f);
		} else {
			fclose(f);
			f = NULL;
		}
	}

	// Allocate memory
	romdata = calloc(poduleromsize, 1);
	if (romdata == NULL) {
		fatal("Out of Memory");
	}

	romdata[0] = 0; // Acorn comformant card, not requesting FIQ, not requesting interupt, EcID = 0 = EcID is extended (8 bytes)
	romdata[1] = 3; // Interrupt status has been relocated, chunk directories present, byte access
	romdata[2] = 0; // Mandatory
	romdata[3] = 3; // Product type, low,  Ethernet
	romdata[4] = 0; // Product type, high, Ethernet
	romdata[5] = 0; // Manufacturer, low,  Acorn UK
	romdata[6] = 0; // Manufacturer, high, Acorn UK
	romdata[7] = 0; // Reserved

	memcpy(romdata + filebase, description, sizeof(description));
	makechunk(0xf5, filebase, sizeof(description)); // 0xf5 = Device Data, Description
	filebase += (sizeof(description) + 3) & ~3u;

	// If the driver module was opened successfully, load it and add to the podule
	if (f != NULL) {
		size_t len = fread(romdata + filebase, 1, module_file_size, f);
		fclose(f);

		if (len == module_file_size) { // Load was OK
			len = (len + 3) & ~3u;
			makechunk(0x81, filebase, (uint32_t) len); // 0x81 = RISC OS, ROM
		}
	}
}

/**
 * Podule byte read function for Ethernet podule
 *
 * @param p    podule pointer (unused)
 * @param easi Read from EASI space or from regular IO space
 * @param addr Address of byte to read
 * @return Contents of byte
 */
static uint8_t
readpoduleetherrpcem(podule *p, int easi, uint32_t addr)
{
	NOT_USED(p);

	if (easi && (poduleromsize > 0)) {
		addr = (addr & 0xffffff) >> 2;
		if (addr < poduleromsize) {
			return romdata[addr];
		}
		return 0x00;
	}
	return 0xff;
}

/**
 * Copy bytes from emulated memory map to host
 *
 * @param dest Pointer to storage in host memory
 * @param src  Memory address in emulated memory map
 * @param len  Amount in bytes to copy
 */
void
memcpytohost(void *dest, uint32_t src, uint32_t len)
{
	char *dst = dest;

	while (len--) {
		*dst++ = mem_read8(src);
		src++;
	}
}

/**
 * Copy bytes from host to emulated memory
 *
 * @param dest   Memory address in emulated memory map
 * @param source Pointer to storage in host memory
 * @param len    Amount in bytes to copy
 */
void
memcpyfromhost(uint32_t dest, const void *source, uint32_t len)
{
	const char *src = source;

	while (len--) {
		mem_write8(dest, *src);
		src++;
		dest++;
	}
}

/**
 * Copy null-terminated string from host to emulated memory
 *
 * @param dest   Memory address in emulated memory map
 * @param source Pointer to null-terminated string
 */
void
strcpyfromhost(uint32_t dest, const char *source)
{
	memcpyfromhost(dest, source, strlen(source) + 1);
}

/**
 * Handle setting new values of networking parameters, and let caller
 * know if they have altered enough to require an emulated machine restart.
 *
 * @param networktype New network type value
 * @param bridgename  String of new bridgename value (zero terminated), caller owns (copy taken)
 * @param ipaddress   String of new ipaddress value (zero terminated), caller owns (copy taken)
 * @return Non-zero if these config changes require an emulator restart
 */
int
network_config_changed(NetworkType network_type, const char *bridgename,
                       const char *ipaddress)
{
	int restart_required = 0;

	if (network_type != config.network_type) {
		config.network_type = network_type;
		restart_required = 1;
	}

	if (bridgename == NULL && config.bridgename != NULL) {
		/* Turned off */
		free(config.bridgename);
		config.bridgename = NULL;
		if (config.network_type == NetworkType_EthernetBridging) {
			restart_required = 1;
		}
	} else if (bridgename != NULL && config.bridgename == NULL) {
		/* Turned on */
		config.bridgename = strdup(bridgename);
		if (config.network_type == NetworkType_EthernetBridging) {
			restart_required = 1;
		}
	} else {
		if (bridgename != NULL && config.bridgename != NULL &&
		    strcmp(bridgename, config.bridgename) != 0)
		{
			/* Bridgename changed */
			free(config.bridgename);
			config.bridgename = strdup(bridgename);
			if (config.network_type == NetworkType_EthernetBridging) {
				restart_required = 1;
			}
		}
	}

	if (ipaddress == NULL && config.ipaddress != NULL) {
		/* Turned off */
		free(config.ipaddress);
		config.ipaddress = NULL;
		if (config.network_type == NetworkType_IPTunnelling) {
			restart_required = 1;
		}
	} else if (ipaddress != NULL && config.ipaddress == NULL) {
		/* Turned on */
		config.ipaddress = strdup(ipaddress);
		if (config.network_type == NetworkType_IPTunnelling) {
			restart_required = 1;
		}
	} else {
		if (ipaddress != NULL && config.ipaddress != NULL &&
		    strcmp(ipaddress, config.ipaddress) != 0)
		{
			/* ipaddress changed */
			free(config.ipaddress);
			config.ipaddress = strdup(ipaddress);
			if (config.network_type == NetworkType_IPTunnelling) {
				restart_required = 1;
			}
		}
	}

	return restart_required;
}

void
network_init(void)
{
	int success;

	assert(config.network_type == NetworkType_NAT ||
	       config.network_type == NetworkType_EthernetBridging ||
	       config.network_type == NetworkType_IPTunnelling);

	/* Build podule header */
	if (romdata == NULL) { // If not previously initialised
		network_rom_init();
	}

	// Initialise networking
	if (config.network_type == NetworkType_NAT) {
		// Call NAT initialisation code
		success = network_nat_init();
	} else {
		// Call platform's initialisation code
		success = network_plt_init();
	}

	if (!success) {
		error("Networking unavailable");
		return;
	}

	// Register podule
	network_poduleinfo = addpodule(NULL, NULL, NULL, NULL, NULL, readpoduleetherrpcem, NULL, NULL, 0);
	if (network_poduleinfo == NULL) {
		error("No free podule for networking");
	}
}

/**
 * Shutdown any running network components.
 *
 * Called on program shutdown and program reset after
 * configuration has changed.
 */
void
network_reset(void)
{
	if (config.network_type == NetworkType_NAT) {
		// Call NAT reset code
		network_nat_reset();
	} else {
		// Call platform's reset code
		network_plt_reset();
	}
}


/**
 * @param      r0    Reason code in r0
 * @param      r1    Pointer to buffer for any error string
 * @param      r2    Reason code dependent
 * @param      r3    Reason code dependent
 * @param      r4    Reason code dependent
 * @param      r5    Reason code dependent
 * @param[out] retr0 Return of r0
 * @param[out] retr1 Return of r1
 *
 * @return 0 in r0 on success, non-0 otherwise and error buffer filled in
 */
void
network_swi(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t *retr0, uint32_t *retr1)
{
#if defined RPCLOG
	rpclog("Network SWI r0 = %d, r2 = %08x\n", r0, r2);
#endif
	switch (r0) {
	case 0:
		if (config.network_type == NetworkType_NAT) {
			*retr0 = network_nat_tx(r1, r2, r3, r4, r5);
		} else {
			*retr0 = network_plt_tx(r1, r2, r3, r4, r5);
		}
		break;
	case 1:
		if (config.network_type == NetworkType_NAT) {
			*retr0 = network_nat_rx(r1, r2, r3, retr1);
		} else {
			*retr0 = network_plt_rx(r1, r2, r3, retr1);
		}
		break;
	case 2:
		if (config.network_type == NetworkType_NAT) {
			network_nat_setirqstatus(r2);
		} else {
			network_plt_setirqstatus(r2);
		}
		*retr0 = 0;
		break;
	case 3:
		if (network_poduleinfo) {
			network_poduleinfo->irq = r2;
		}
		rethinkpoduleints();
		*retr0 = 0;
		break;
	case 4:
		memcpyfromhost(r2, network_hwaddr, sizeof(network_hwaddr));
		*retr0 = 0;
		break;
	default:
		strcpyfromhost(r1, "Unknown RPCEmu network SWI");
		*retr0 = r1;
		break;
	}
}

/**
 * Parse a colon-separated MAC address
 *
 * @param macaddress string of MAC address
 * @param hwaddr     pointer to 6 values to fill in with the MAC details
 * @returns if parsed correct 1, else 0
 */
int
network_macaddress_parse(const char *macaddress, uint8_t hwaddr[6])
{
	uint32_t hwaddr32[6];
	int items;
	unsigned i;

	assert(macaddress != NULL);
	assert(hwaddr != NULL);

	if (strlen(macaddress) != 17) { /* xx:xx:xx:xx:xx:xx */
		return 0;
	}

	for (i = 0; i < 6; i++) {
		/* 6 sections of 2 hex digits */
		if (!isxdigit(macaddress[(i * 3) + 0]) ||
		    !isxdigit(macaddress[(i * 3) + 1]))
		{
			return 0;
		}

		/* 5 sections of : */
		if (i < 5) {
			if (macaddress[(i * 3) + 2] != ':') {
				return 0;
			}
		}
	}

	items = sscanf(macaddress, "%x:%x:%x:%x:%x:%x",
			&hwaddr32[0], &hwaddr32[1], &hwaddr32[2],
			&hwaddr32[3], &hwaddr32[4], &hwaddr32[5]);

	assert(items == 6); /* with pre-parsing it should be impossible for the above sscanf to fail */

	for (i = 0; i < 6; i++) {
		hwaddr[i] = hwaddr32[i];
	}

	return 1;
}
