/* Buddy 分配 Hook 实现 */

#include "buddy_hook.h"
#include "stats.h"

/* Hook 实例 */
static  struct smartmem_hook buddy_hook_inst = {
    .name = "buddy",
    .type = SMARTMEM_HOOK_TYPE_BUDDY,
    .enabled = false,
    .list = LIST_HEAD_INIT(buddy_hook_inst.list),
};

/**
 * Buddy Hook 初始化
 */
int buddy_hook_init(void)
{
    pr_info("smartmem: buddy hook initializing...\n");

    /* TODO: 注册 kprobe/kretprobe */

    pr_info("smartmem: buddy hook initialized\n");
    return 0;
}

/**
 * Buddy Hook 退出
 */
void buddy_hook_exit(void)
{
    pr_info("smartmem: buddy hook exiting...\n");

    /* TODO: 注销 kprobe/kretprobe */

    pr_info("smartmem: buddy hook exited\n");
}

/**
 * 启用 Buddy Hook
 */
int buddy_hook_enable(void)
{
    pr_info("smartmem: buddy hook enabled\n");

    /* TODO: 启用 kprobe/kretprobe */

    return 0;
}

/**
 * 禁用 Buddy Hook
 */
int buddy_hook_disable(void)
{
    pr_info("smartmem: buddy hook disabled\n");
    /* TODO: 禁用 kprobe/kretprobe */
    return 0;
}
