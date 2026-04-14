/* LRU 管理 Hook */

#ifndef _SMARTMEM_LRU_HOOK_H
#define _SMARTMEM_LRU_HOOK_H

#include "hook.h"

/* LRU Hook 接口 */
int lru_hook_init(void);
void lru_hook_exit(void);
int lru_hook_enable(void);
int lru_hook_disable(void);

#endif /* _SMARTMEM_LRU_HOOK_H */