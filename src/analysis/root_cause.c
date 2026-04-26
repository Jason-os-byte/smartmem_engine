/* 根因分析实现 */
#include "root_cause.h"
#include "hotspot.h"
#include "stats.h"
#include <linux/mmzone.h>
#include <linux/swap.h>

static bool root_cause_initialized = false;

/* 当前检测到的根因 */
static struct root_cause_entry rc_entries[MAX_ROOT_CAUSES];
static int rc_count = 0;
static DEFINE_SPINLOCK(rc_lock);

static const char *root_cause_type_str(enum root_cause_type type)
{
    switch (type) {
    case ROOT_CAUSE_KSWAPD_PRESSURE: return "kswapd_pressure";
    case ROOT_CAUSE_DIRECT_RECLAIM:  return "direct_reclaim";
    case ROOT_CAUSE_COMPACT_FAIL:    return "compact_fail";
    case ROOT_CAUSE_SLAB_BLOAT:      return "slab_bloat";
    case ROOT_CAUSE_MISPLACED_NUMA:  return "misplaced_numa";
    default:                         return "unknown";
    }
}

static void add_root_cause(enum root_cause_type type,
                           enum bottleneck_type bottleneck,
                           u64 confidence,
                           const char *desc, const char *suggestion)
{
    struct root_cause_entry *e;
    unsigned long flags;

    spin_lock_irqsave(&rc_lock, flags);

    if (rc_count >= MAX_ROOT_CAUSES) {
        spin_unlock_irqrestore(&rc_lock, flags);
        return;
    }

    e = &rc_entries[rc_count];
    e->type = type;
    e->related_bottleneck = bottleneck;
    e->confidence = confidence;
    strscpy(e->description, desc, sizeof(e->description));
    strscpy(e->suggestion, suggestion, sizeof(e->suggestion));

    rc_count++;

    spin_unlock_irqrestore(&rc_lock, flags);

    pr_info("smartmem-rootcause: [%s] %s (confidence=%llu%%)\n",
            root_cause_type_str(type), desc, confidence);
}

/**
 * 分析 kswapd 压力
 * 如果 kswapd 回收页面多，说明后台回收压力大
 */
static void analyze_kswapd_pressure(struct bottleneck_entry *bottlenecks,
                                     int bn_cnt)
{
    int i;
    bool has_oom = false;

    for (i = 0; i < bn_cnt; i++) {
        if (bottlenecks[i].type == BOTTLENECK_OOM_RISK)
            has_oom = true;
    }

    if (has_oom) {
        unsigned long total_ram = totalram_pages();
        unsigned long free_ram = global_zone_page_state(NR_FREE_PAGES);
        int free_pct = 100;

        if (total_ram > 0)
            free_pct = (int)((free_ram * 100) / total_ram);

        /* 空闲率低于 15% 即认为 kswapd 有压力 */
        if (free_pct < 15) {
            char desc[128];
            snprintf(desc, sizeof(desc),
                     "Memory pressure: only %d%% free, kswapd likely active",
                     free_pct);
            add_root_cause(ROOT_CAUSE_KSWAPD_PRESSURE,
                           BOTTLENECK_OOM_RISK, 80,
                           desc,
                           "Consider adding memory or reducing workload");
        }
    }
}

/**
 * 分析直接回收
 * 直接回收意味着分配路径中同步回收，延迟会很高
 */
static void analyze_direct_reclaim(struct bottleneck_entry *bottlenecks,
                                    int bn_cnt)
{
    int i;
    bool has_slow = false;

    for (i = 0; i < bn_cnt; i++) {
        if (bottlenecks[i].type == BOTTLENECK_SLOW_ALLOC)
            has_slow = true;
    }

    if (has_slow) {
        unsigned long total_ram = totalram_pages();
        unsigned long free_ram = global_zone_page_state(NR_FREE_PAGES);
        int free_pct = 100;

        if (total_ram > 0)
            free_pct = (int)((free_ram * 100) / total_ram);

        /* 空闲率低于 10% 很可能触发直接回收 */
        if (free_pct < 10) {
            char desc[128];
            snprintf(desc, sizeof(desc),
                     "Direct reclaim likely: only %d%% free memory",
                     free_pct);
            add_root_cause(ROOT_CAUSE_DIRECT_RECLAIM,
                           BOTTLENECK_SLOW_ALLOC, 85,
                           desc,
                           "Tune min_free_kbytes or reduce memory pressure");
        }
    }
}

/**
 * 分析碎片导致整理失败
 */
static void analyze_compact_fail(struct bottleneck_entry *bottlenecks,
                                  int bn_cnt)
{
    int i;
    bool has_frag = false;

    for (i = 0; i < bn_cnt; i++) {
        if (bottlenecks[i].type == BOTTLENECK_HIGH_FRAGMENT)
            has_frag = true;
    }

    if (has_frag) {
        add_root_cause(ROOT_CAUSE_COMPACT_FAIL,
                       BOTTLENECK_HIGH_FRAGMENT,
                       75,
                       "Memory compaction insufficient for fragmentation",
                       "Trigger manual compaction or increase min_free_kbytes");
    }
}

/**
 * 分析 NUMA 放置问题
 */
static void analyze_numa_misplace(struct bottleneck_entry *bottlenecks,
                                   int bn_cnt)
{
    int i;
    bool has_numa = false;

    for (i = 0; i < bn_cnt; i++) {
        if (bottlenecks[i].type == BOTTLENECK_NUMA_IMBALANCE)
            has_numa = true;
    }

    if (has_numa) {
        add_root_cause(ROOT_CAUSE_MISPLACED_NUMA,
                       BOTTLENECK_NUMA_IMBALANCE,
                       70,
                       "Pages placed on suboptimal NUMA node",
                       "Enable NUMA balancing or use numactl for workload");
    }
}

/**
 * 更新根因分析
 */
int root_cause_update(void)
{
    struct bottleneck_entry bottlenecks[MAX_BOTTLENECKS];
    int bn_cnt;
    unsigned long flags;

    if (!root_cause_initialized)
        return -ENODEV;

    /* 清空旧结果 */
    spin_lock_irqsave(&rc_lock, flags);
    rc_count = 0;
    spin_unlock_irqrestore(&rc_lock, flags);

    /* 先获取瓶颈数据 */
    bn_cnt = bottleneck_get_entries(bottlenecks, MAX_BOTTLENECKS);
    if (bn_cnt == 0)
        return 0;

    /* 基于瓶颈进行根因推断 */
    analyze_kswapd_pressure(bottlenecks, bn_cnt);
    analyze_direct_reclaim(bottlenecks, bn_cnt);
    analyze_compact_fail(bottlenecks, bn_cnt);
    analyze_numa_misplace(bottlenecks, bn_cnt);

    return 0;
}

/**
 * 获取根因分析结果
 */
int root_cause_get_entries(struct root_cause_entry *entries, int max)
{
    unsigned long flags;
    int count, i;

    spin_lock_irqsave(&rc_lock, flags);
    count = min(rc_count, max);
    for (i = 0; i < count; i++)
        entries[i] = rc_entries[i];
    spin_unlock_irqrestore(&rc_lock, flags);

    return count;
}

/**
 * 根因分析初始化
 */
int root_cause_init(void)
{
    pr_info("smartmem: root cause analysis initializing...\n");

    rc_count = 0;
    root_cause_initialized = true;

    pr_info("smartmem: root cause analysis initialized\n");
    return 0;
}

/**
 * 根因分析退出
 */
void root_cause_exit(void)
{
    pr_info("smartmem: root cause analysis exiting...\n");

    rc_count = 0;
    root_cause_initialized = false;

    pr_info("smartmem: root cause analysis exited\n");
}