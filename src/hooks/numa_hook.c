/* NUMA 管理 Hook 实现 */

#include "numa_hook.h"
#include "stats.h"

/* Hook 实例 */
static struct smartmem_hook numa_hook_inst = {
    .name = "numa",
    .type = SMARTMEM_HOOK_TYPE_NUMA,
    .enabled = false,
    .list = LIST_HEAD_INIT(numa_hook_inst.list),
};

/**
 * NUMA Hook 初始化
 */
int numa_hook_init(void)
{
    pr_info("smartmem: numa hook initializing...\n");
    pr_info("smartmem: numa hook initialized\n");
    return 0;
}

/**
 * NUMA Hook 退出
 */
void numa_hook_exit(void)
{
    pr_info("smartmem: numa hook exiting...\n");
    pr_info("smartmem: numa hook exited\n");
}

/**
 * 启用 NUMA Hook
 */
int numa_hook_enable(void)
{
    pr_info("smartmem: numa hook enabled\n");
    return 0;
}

/**
 * 禁用 NUMA Hook
 */
int numa_hook_disable(void)
{
    pr_info("smartmem: numa hook disabled\n");
    return 0;
}
