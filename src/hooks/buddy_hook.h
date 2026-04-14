/* Buddy 分配 Hook */

#ifndef _SMARTBUDDY_HOOK_H
#define _SMARTBUDDY_HOOK_H

#include "hook.h"

/* Buddy Hook 接口 */
int buddy_hook_init(void);
void buddy_hook_exit(void);
int buddy_hook_enable(void);
int buddy_hook_disable(void);

#endif /* _SMARTBUDDY_HOOK_H */