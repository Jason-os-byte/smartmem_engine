/* VMA 管理 Hook */
#ifndef _SMARTMEM_VMA_HOOK_H
#define _SMARTMEM_VMA_HOOK_H

#include "hook.h"

/* VMA Hook 接口 */
int vma_hook_init(void);
void vma_hook_exit(void);
int vma_hook_enable(void);
int vma_hook_disable(void);

#endif /* _SMARTMEM_VMA_HOOK_H */