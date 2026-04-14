/* LRU 管理 Hook 实现 */

#include "lru_hook.h"
#include "../core/stats.h"

/* Hook 实例 */
static struct smartmem_hook lru_hook_inst = {
    .name = "lru",
    .type = SMARTMEM_HOOK_TYPE_LRU,
    .enabled = false,
    .list = LIST_HEAD_INIT(lru_hook_inst.list),
};

/**
 * LRU Hook 初始化
 */
int lru_hook_init(void)
{
    pr_info("smartmem: lru hook initializing...\n");
    pr_info("smartmem: lru hook initialized\n");
    return 0;
}

/**
 * LRU Hook 退出
 */
void lru_hook_exit(void)
{
    pr_info("smartmem: lru hook exiting...\n");
    pr_info("smartmem: lru hook exited\n");
}

/**
 * 启用 LRU Hook
 */
int lru_hook_enable(void)
{
    pr_info("smartmem: lru hook enabled\n");
    return 0;
}

/**
 * 禁用 LRU Hook
 */
int lru_hook_disable(void)
{
    pr_info("smartmem: lru hook disabled\n");
    return 0;
}
