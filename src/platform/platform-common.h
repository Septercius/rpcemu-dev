#ifndef PLATFORM_COMMON_H
#define PLATFORM_COMMON_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <stdbool.h>
#include "rpcemu.h"

typedef enum
{
    StartupModifierKey_Alt = (1 << 0),
    StartupModifierKey_Control = (1 << 1),
    StartupModifierKey_Meta = (1 << 2),
    StartupModifierKey_Shift = (1 << 3)
} StartupModifierKey;

typedef struct
{
    uint64_t size; /**< Size of disk */
    uint64_t free; /**< Free space on disk */
} disk_info;

extern int path_disk_info(const char *path, disk_info *d);

extern void rpcemu_log_os(void);

extern const char *rpcemu_get_datadir(void);
extern const char *rpcemu_get_log_path(void);

extern bool rpcemu_platform_startup(int modifierMask);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
