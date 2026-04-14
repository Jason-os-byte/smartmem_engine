/* hooks/vma_hook.c - VMA 管理 Hook 实现 */

#include "vma_hook.h"
#include "stats.h"

/* Hook 实例 */
static struct smartmem_hook vma_hook_inst = {
    .name = "vma",
    .type = SMARTMEM_HOOK_TYPE_VMA,
    .enabled = false,
    .list = LIST_HEAD_INIT(vma_hook_inst.list),
};

/**
 * VMA Hook 初始化
 */
int vma_hook_init(void)
{
    pr_info("smartmem: vma hook initializing...\n");
    pr_info("smartmem: vma hook initialized\n");
    return 0;
}

/**
 * VMA Hook 退出
 */
void vma_hook_exit(void)
{
    pr_info("smartmem: vma hook exiting...\n");
    pr_info("smartmem: vma hook exited\n");
}

/**
 * 启用 VMA Hook
 */
int vma_hook_enable(void)
{
    pr_info("smartmem: vma hook enabled\n");
    return 0;
}

/**
 * 禁用 VMA Hook
 */
int vma_hook_disable(void)
{
    pr_info("smartmem: vma hook disabled\n");
    return 0;
}