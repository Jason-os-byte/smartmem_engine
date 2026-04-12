/* smartmem.c - 智能内存优化引擎模块主入口 */

#include "smartmem.h"

/* 全局状态 */
struct smartmem_global_state *g_sm_state = NULL;

/**
 * smartmem_init - 模块初始化 
 */
static int __init smartmem_init(void)
{
    pr_info("%s: %s initializing...\n", SMARTMEM_NAME, SMARTMEM_DESC);

    /* 分配全局状态 */
    g_sm_state = kzalloc(sizeof(*g_sm_state), GFP_KERNEL);
    if (!g_sm_state) {
        pr_err("%s: failed to allocate global state\n", SMARTMEM_NAME);
        return -ENOMEM;
    }

    spin_lock_init(&g_sm_state->lock);
    g_sm_state->state = SMARTMEM_STATE_INITIALIZED;
    g_sm_state->enabled = false;
    g_sm_state->active_hooks = 0;

    /* TODO: 调用子模块初始化 (后续阶段实现) */

    g_sm_state->state = SMARTMEM_STATE_READY;

    pr_info("%s: initialized successfully\n", SMARTMEM_NAME);
    return 0;
}

/**
 * smartmem_exit - 模块退出
 */
static void __exit smartmem_exit(void)
{
    if (!g_sm_state) {
        return;
    }

    pr_info("%s: exiting...\n", SMARTMEM_NAME);

    g_sm_state->state = SMARTMEM_STATE_STOPPING;

    /* TODO: 调用子模块清理（后续阶段实现）*/

    g_sm_state->state = SMARTMEM_STATE_UNINITIALIZED;

    kfree(g_sm_state);
    g_sm_state = NULL;

    pr_info("%s: exited\n", SMARTMEM_NAME);
}

module_init(smartmem_init);
module_exit(smartmem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jason");
MODULE_DESCRIPTION(SMARTMEM_DESC);
MODULE_VERSION(SMARTMEM_VERSION);