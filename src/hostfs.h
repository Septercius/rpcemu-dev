/*
 * $Id: hostfs.h,v 1.1 2005/07/27 21:34:34 mhowkins Exp $
 */

#ifndef HOSTFS_H
#define HOSTFS_H

//#include "armdefs.h"
#include "rpcemu.h"

#define ARCEM_SWI_CHUNK    0x56ac0
#define ARCEM_SWI_SHUTDOWN  (ARCEM_SWI_CHUNK + 0)
#define ARCEM_SWI_HOSTFS    (ARCEM_SWI_CHUNK + 1)
#define ARCEM_SWI_DEBUG     (ARCEM_SWI_CHUNK + 2)
//#define ARCEM_SWI_NANOSLEEP (ARCEM_SWI_CHUNK + 3)	/* Reserved */
#define ARCEM_SWI_NETWORK   (ARCEM_SWI_CHUNK + 4)

typedef uint32_t ARMword;
typedef struct {
  uint32_t *Reg;
} ARMul_State;


extern void hostfs(ARMul_State *state);
extern void hostfs_init(void);
extern void hostfs_reset(void);

#define ARMul_LoadWordS(state, address) readmeml(address)
#define ARMul_LoadByte(state, address) readmemb(address)
#define ARMul_StoreWordS(state, address, data) writememl(address, data)
#define ARMul_StoreByte(state, address, data) writememfb(address, data)

#endif
