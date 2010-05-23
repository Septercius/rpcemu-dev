/*
 network.c - code shared between each host platforms networking support
 */
#include <assert.h>
#include <string.h>

#include "rpcemu.h"
#include "mem.h"

void
memcpytohost(void *dest, uint32_t src, uint32_t len)
{
	char *dst = dest;

	while (len--) {
		*dst++ = readmemb(src);
		src++;
	}
}

void
memcpyfromhost(uint32_t dest, const void *source, uint32_t len)
{
	const char *src = source;

	while (len--) {
		writememb(dest, *src);
		src++;
		dest++;
	}
}

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

	assert(network_type < NetworkType_MAX);

	if (network_type != config.network_type) {
		config.network_type = network_type;
		restart_required = 1;
	}

	if (bridgename == NULL && config.bridgename != NULL) {
		/* Turned off */
		free(config.bridgename);
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
