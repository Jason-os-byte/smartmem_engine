/* SLUB 分配 Hook */

#ifndef _SMARTMEM_SLUB_HOOK_H
#define _SMARTMEM_SLUB_HOOK_H

#include "hook.h"

/**
 * 用于保存 SLUB 分配参数
 */
struct slub_alloc_args {
	size_t size;
	gfp_t gfp_mask;
	ktime_t start_time;
	bool active;
};

/* SLUB Hook 接口 */
int slub_hook_init(void);
void slub_hook_exit(void);
int slub_hook_enable(void);
int slub_hook_disable(void);

#endif /* _SMARTMME_SLUB_HOOK_H */