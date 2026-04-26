/* 优化引擎 */

#include "optimization.h"
#include "auto_tune.h"
#include "predictive.h"
#include <linux/ktime.h>

static struct smartmem_optimization g_optimization = {
    .auto_tune_enabled = false,
    .predictive_enabled = false,
    .initialized = false,
};

/**
 * 优化引擎初始化
 */
int smartmem_optimization_init(void)
{
    int ret;

    pr_info("smartmem: optimization engine initializing...\n");

    /* 初始化自动调优 */
    ret = auto_tune_init();
    if (ret) {
        pr_warn("smartmem: auto tune init failed\n");
    } else {
        g_optimization.auto_tune_enabled = true;
    }

     /* 初始化预测模型 */
    ret = predictive_init();
    if (ret) {
        pr_warn("smartmem: predictive init failed\n");
    } else {
        g_optimization.predictive_enabled = true;
    }

    g_optimization.initialized = true;

    pr_info("smartmem: optimization engine initialized (auto_tune=%s, predictive=%s)\n",
            g_optimization.auto_tune_enabled ? "yes" : "no",
            g_optimization.predictive_enabled ? "yes" : "no");
    return 0;
}

/**
 * 优化引擎退出
 */
void smartmem_optimization_exit(void)
{
    pr_info("smartmem: optimization engine exiting...\n");

    if (g_optimization.predictive_enabled) {
        predictive_exit();
        g_optimization.predictive_enabled = false;
    }

    if (g_optimization.auto_tune_enabled) {
        auto_tune_exit();
        g_optimization.auto_tune_enabled = false;
    }

    g_optimization.initialized = false;

    pr_info("smartmem: optimization engine exited\n");
}

/**
 * 启动优化
 */
int smartmem_optimization_start(void)
{
    int ret;

    pr_info("smartmem: starting optimization...\n");

    if (g_optimization.auto_tune_enabled) {
        ret = auto_tune_start();
        if (ret) {
            pr_warn("smartmem: failed to start auto tune\n");
        }
    }

    if (g_optimization.predictive_enabled) {
        ret = predictive_start();
        if (ret) {
            pr_warn("smartmem: failed to start predictive\n");
        }
    }

    pr_info("smartmem: optimization started\n");
    return 0;
}

/**
 * 停止优化
 */
int smartmem_optimization_stop(void)
{
    pr_info("smartmem: stopping optimization...\n");

    if (g_optimization.predictive_enabled) {
        predictive_stop();
    }

    if (g_optimization.auto_tune_enabled) {
        auto_tune_stop();
    }

    pr_info("smartmem: optimization stopped\n");
    return 0;
}

int smartmem_optimization_tune_check(void)
{
    if (!g_optimization.auto_tune_enabled)
        return -ENODEV;
    return auto_tune_check();
}

int smartmem_optimization_tune_trigger(int action)
{
    if (!g_optimization.auto_tune_enabled)
        return -ENODEV;
    return auto_tune_trigger((enum tune_action)action);
}

void smartmem_optimization_get_tune_stats(struct tune_stats *stats)
{
    auto_tune_get_stats(stats);
}

int smartmem_optimization_get_tune_history(struct tune_history *entries, int max)
{
    return auto_tune_get_history(entries, max);
}
