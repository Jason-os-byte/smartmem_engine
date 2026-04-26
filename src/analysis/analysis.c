/* 分析引擎实现 */
#include "analysis.h"
#include "hotspot.h"
#include "bottleneck.h"
#include "root_cause.h"

static struct smartmem_analysis g_analysis = {
    .hotspot_enabled = false,
    .bottleneck_enabled = false,
    .root_cause_enabled = false,
    .initialized = false,
};

/* 分析引擎初始化 */
int smartmem_analysis_init(void)
{
    int ret;

    pr_info("smartmem: analysis engine initializing...\n");

    /* 初始化热点识别 */
    ret = hotspot_init();
    if (ret) {
        pr_warn("smartmem: hotspot init failed\n");
    } else {
        g_analysis.hotspot_enabled = true;
    }

    /* 初始化瓶颈分析 */
    ret = bottleneck_init();
    if (ret) {
        pr_warn("smartmem: bottleneck init failed\n");
    } else {
        g_analysis.bottleneck_enabled = true;
    }

    /* 初始化根因分析 */
    ret = root_cause_init();
    if (ret) {
        pr_warn("smartmem: root cause init failed\n");
    } else {
        g_analysis.root_cause_enabled = true;
    }

    g_analysis.initialized = true;

    pr_info("smartmem: analysis engine initialized (hotspot=%s, bottleneck=%s, root_cause=%s)\n",
            g_analysis.hotspot_enabled ? "yes" : "no",
            g_analysis.bottleneck_enabled ? "yes" : "no",
            g_analysis.root_cause_enabled ? "yes" : "no");
    return 0;
}

/* 分析引擎退出 */
void smartmem_analysis_exit(void)
{
    pr_info("smartmem: analysis engine exiting...\n");

    if (g_analysis.root_cause_enabled) {
        root_cause_exit();
        g_analysis.root_cause_enabled = false;
    }

    if (g_analysis.bottleneck_enabled) {
        bottleneck_exit();
        g_analysis.bottleneck_enabled = false;
    }

    if (g_analysis.hotspot_enabled) {
        hotspot_exit();
        g_analysis.hotspot_enabled = false;
    }

    g_analysis.initialized = false;

    pr_info("smartmem: analysis engine exited\n");
}

/**
 * 更新热点分析
 */
int smartmem_analysis_hotspot_update(void)
{
    if (!g_analysis.hotspot_enabled) {
        return -ENODEV;
    }

    return hotspot_update();
}

/**
 * 更新瓶颈分析
 */
int smartmem_analysis_bottleneck_update(void)
{
    if (!g_analysis.bottleneck_enabled) {
        return -ENODEV;
    }

    return bottleneck_update();
}

/**
 * 更新根因分析
 */
int smartmem_analysis_root_cause_update(void)
{
    if (!g_analysis.root_cause_enabled) {
        return -ENODEV;
    }

    return root_cause_update();
}

int smartmem_analysis_get_bottlenecks(struct bottleneck_entry *entries, int n)
{
    if (!g_analysis.bottleneck_enabled)
        return -ENODEV;

    return bottleneck_get_entries(entries, n);
}

int smartmem_analysis_get_root_causes(struct root_cause_entry *entries, int n)
{
    if (!g_analysis.root_cause_enabled)
        return -ENODEV;

    return root_cause_get_entries(entries, n);
}

int smartmem_analysis_get_hotspots(struct hotspot_top_entry *entries, int n)
{
    if (!g_analysis.hotspot_enabled)
        return -ENODEV;

    return hotspot_get_top_n(entries, n);
}
