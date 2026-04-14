/* SLUB 分配 Hook 实现 */
#include "slub_hook.h"
#include "stats.h"

/* Hook 实例 */
static struct smartmem_hook slub_hook_inst = {
    .name = "slub",
    .type = SMARTMEM_HOOK_TYPE_SLUB,
    .enabled = false,
    .list = LIST_HEAD_INIT(slub_hook_inst.list),
};

/**
 * SLUB Hook 初始化
 */
int slub_hook_init(void)
{
    pr_info("smartmem: slub hook initializing...\n");

    /* TODO: 注册 kprobe/kretprobe */

    pr_info("smartmem: slub hook initialized\n");
    return 0;
}

/**
 * SLUB Hook 退出
 */
void slub_hook_exit(void)
{
    pr_info("smartmem: slub hook exiting...\n");

    /* TODO: 注销 kprobe/kretprobe */

    pr_info("smartmem: slub hook exited\n");
}

/**
 * 启用 SLUB Hook
 */
int slub_hook_enable(void)
{
    pr_info("smartmem: slub hook enabled\n");
    /* TODO: 启用 kprobe/kretprobe */
    return 0;
}

/**
 * 禁用 SLUB Hook
 */
int slub_hook_disable(void)
{
    pr_info("smartmem: slub hook disabled\n");
    /* TODO: 禁用 kprobe/kretprobe */
    return 0;
}





