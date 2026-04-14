/* NUMA 管理 Hook */

#ifndef _SMARTMEM_NUMA_HOOK_H
#define _SMARTMEM_NUMA_HOOK_H

#include "hook.h"

/* NUMA Hook 接口 */
int numa_hook_init(void);
void numa_hook_exit(void);
int numa_hook_enable(void);
int numa_hook_disable(void);

#endif /* _SMARTMEM_NUMA_HOOK_H */
