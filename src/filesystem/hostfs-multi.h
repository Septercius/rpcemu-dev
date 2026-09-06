/*
 * $Id: hostfs-multi.h,v 1.1 2005/07/27 21:34:34 mhowkins Exp $
 */

#ifndef HOSTFS_MULTI_H
#define HOSTFS_MULTI_H

#include "rpcemu.h"

#define ARCEM_SWI_CHUNK 0x56ac0
#define ARCEM_SWI_SHUTDOWN (ARCEM_SWI_CHUNK + 0)
#define ARCEM_SWI_HOSTFS (ARCEM_SWI_CHUNK + 1)
#define ARCEM_SWI_DEBUG (ARCEM_SWI_CHUNK + 2)
// #define ARCEM_SWI_NANOSLEEP (ARCEM_SWI_CHUNK + 3)    /* Reserved */
#define ARCEM_SWI_NETWORK (ARCEM_SWI_CHUNK + 4)

#define ARCEM_SWI_MULTI_HOSTFS_CHUNK_START (ARCEM_SWI_CHUNK + 0x10)
#define ARCEM_SWI_MULTI_HOSTFS_INITIALISE (ARCEM_SWI_CHUNK + 0x10)
#define ARCEM_SWI_MULTI_HOSTFS_DRIVES (ARCEM_SWI_CHUNK + 0x11)
#define ARCEM_SWI_MULTI_HOSTFS_GET_DRIVE_NAME (ARCEM_SWI_CHUNK + 0x12)
#define ARCEM_SWI_MULTI_HOSTFS_GENERATE_COMMAND (ARCEM_SWI_CHUNK + 0x13)
#define ARCEM_SWI_MULTI_HOSTFS_FREEOP (ARCEM_SWI_CHUNK + 0x14)
#define ARCEM_SWI_MULTI_HOSTFS_VALIDATE_DRIVE (ARCEM_SWI_CHUNK + 0x15)
#define ARCEM_SWI_MULTI_HOSTFS_CHUNK_END (ARCEM_SWI_CHUNK + 0x15)

typedef uint32_t ARMword;

typedef struct
{
    uint32_t *Reg;
} ARMul_State;

extern void multi_hostfs_reset(void);
extern void multi_hostfs_swi_dispatch(int swinum, ARMul_State *state);

#define ARMul_LoadByte(state, address) mem_read8(address)
#define ARMul_LoadWordS(state, address) mem_read32(address)
#define ARMul_StoreByte(state, address, data) mem_write8(address, data)
#define ARMul_StoreWordS(state, address, data) mem_write32(address, data)

#endif
