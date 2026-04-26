/* smartmem.c - 智能内存优化引擎模块主入口 */

#include "smartmem.h"
#include "engine.h"
#include "config.h"
#include "stats.h"
#include "hook.h"
#include "buddy_hook.h"
#include "slub_hook.h"
#include "vma_hook.h"
#include "lru_hook.h"
#include "numa_hook.h"
#include "strategy.h"
#include "monitor.h"
#include "analysis.h"
#include "optimization.h"
#include "interface.h"


/* 全局状态 */
struct smartmem_global_state *g_sm_state = NULL;

/**
 * smartmem_init - 模块初始化 
 */
static int __init smartmem_init(void)
{
    int ret;

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

    /* 初始化核心模块 */
    ret = smartmem_config_init();
    if (ret) {
        pr_err("%s: config init failed\n", SMARTMEM_NAME);
        goto err_config;
    }

    ret = smartmem_stats_init();
    if (ret) {
        pr_err("%s: stats init failed\n", SMARTMEM_NAME);
        goto err_stats;
    }

    ret = smartmem_engine_init();
    if (ret) {
        pr_err("%s: engine init failed\n", SMARTMEM_NAME);
        goto err_engine;
    }

    /* 初始化 Hook 管理框架（会自动调用所有子 hook） */
    ret = smartmem_hook_init();
    if (ret) {
        pr_err("%s: hook init failed\n", SMARTMEM_NAME);
        goto err_hook;
    }

    /* 初始化策略引擎 */
    ret = smartmem_strategy_init();
    if (ret) {
        pr_err("%s: strategy init failed\n", SMARTMEM_NAME);
        goto err_strategy;
    }

    /* 初始化监控系统 */
    ret = smartmem_monitor_init();
    if (ret) {
        pr_err("%s: monitor init failed\n", SMARTMEM_NAME);
        goto err_monitor;
    }

    /* 启动监控系统 */
    ret = smartmem_monitor_start();
    if (ret) {
        pr_warn("%s: monitor start failed, continuing without monitoring\n", SMARTMEM_NAME);
    }

    /* 初始化分析引擎 */
    ret = smartmem_analysis_init();
    if (ret) {
        pr_err("%s: analysis init failed\n", SMARTMEM_NAME);
        goto err_analysis;
    }

    /* 初始化优化引擎 */
    ret = smartmem_optimization_init();
    if (ret) {
        pr_err("%s: optimization init failed\n", SMARTMEM_NAME);
        goto err_optimization;
    }

    /* 初始化接口层 */
    ret = smartmem_interface_init();
    if (ret) {
        pr_err("%s: interface init failed\n", SMARTMEM_NAME);
        goto err_interface;
    }

    g_sm_state->state = SMARTMEM_STATE_READY;

    pr_info("%s: initialized successfully\n", SMARTMEM_NAME);
    return 0;

err_interface:
    smartmem_optimization_exit();
err_optimization:
    smartmem_analysis_exit();
err_analysis:
    smartmem_monitor_exit();
err_monitor:
    smartmem_strategy_exit();
err_strategy:
    smartmem_hook_exit();
err_hook:
    smartmem_engine_exit();
err_engine:
    smartmem_stats_exit();
err_stats:
    smartmem_config_exit();
err_config:
    kfree(g_sm_state);
    g_sm_state = NULL;
    return ret;
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

    smartmem_interface_exit();
    smartmem_optimization_exit();
    smartmem_analysis_exit();
    smartmem_monitor_stop();
    smartmem_monitor_exit();
    smartmem_strategy_exit();
    smartmem_hook_exit();
    smartmem_engine_exit();
    smartmem_stats_exit();
    smartmem_config_exit();

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