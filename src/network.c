/*
 network.c - code shared between each host platforms networking support
 */
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
