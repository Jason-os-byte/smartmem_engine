/* Buddy 分配 Hook */

#ifndef _SMARTBUDDY_HOOK_H
#define _SMARTBUDDY_HOOK_H

#include "hook.h"
#include <linux/kprobes.h>

/* Buddy Hook 接口 */
int buddy_hook_init(void);
void buddy_hook_exit(void);
int buddy_hook_enable(void);
int buddy_hook_disable(void);

/* per-CPU 参数缓存 */
struct buddy_alloc_args {
    int order;
    gfp_t gfp_mask;
    ktime_t start_time;
    bool active;
};

#endif /* _SMARTBUDDY_HOOK_H */