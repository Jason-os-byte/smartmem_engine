/* 瓶颈分析实现 */

#include "bottleneck.h"
#include "hotspot.h"
#include "stats.h"
#include "adaptive_slub.h"
#include <linux/slab.h>
#include <linux/mmzone.h>
#include <linux/cpumask.h>
#include <linux/mm.h>
#include <linux/swap.h>

static bool bottleneck_initialized = false;

/* 瓶颈阈值配置 */
struct bottleneck_thresholds {
    u64 slow_alloc_ns;           /* 慢分配延迟阈值(ns) */
    u64 slow_alloc_count;        /* 慢分配次数阈值 */
    u64 fragment_percent;        /* 碎片率阈值(%) */
    u64 numa_imbalance_percent;  /* NUMA 不平衡阈值(%) */
    u64 slab_hit_low_percent;    /* SLAB 低命中率阈值(%) */
    u64 oom_free_percent;        /* OOM 风险空闲率阈值(%) */
};

static struct bottleneck_thresholds bn_thresholds = {
    .slow_alloc_ns          = 1000000,    /* 1ms */
    .slow_alloc_count       = 100,
    .fragment_percent       = 30,
    .numa_imbalance_percent = 20,
    .slab_hit_low_percent   = 60,
    .oom_free_percent       = 10,
};

/* 当前检测到的瓶颈 */
static struct bottleneck_entry bn_entries[MAX_BOTTLENECKS];
static int bn_count = 0;
static DEFINE_SPINLOCK(bn_lock);

static const char *bottleneck_type_str(enum bottleneck_type type)
{
    switch (type) {
    case BOTTLENECK_SLOW_ALLOC:    return "slow_alloc";
    case BOTTLENECK_HIGH_FRAGMENT: return "high_fragment";
    case BOTTLENECK_NUMA_IMBALANCE:return "numa_imbalance";
    case BOTTLENECK_LOW_SLAB_HIT:  return "low_slab_hit";
    case BOTTLENECK_OOM_RISK:      return "oom_risk";
    default:                       return "unknown";
    }
}

static const char *severity_str(enum bottleneck_severity sev)
{
    switch (sev) {
    case SEVERITY_LOW:      return "low";
    case SEVERITY_MEDIUM:   return "medium";
    case SEVERITY_HIGH:     return "high";
    case SEVERITY_CRITICAL: return "critical";
    default:                return "unknown";
    }
}

/**
 * 添加一个瓶颈条目
 */
static void add_bottleneck(enum bottleneck_type type,
                           enum bottleneck_severity severity,
                           u64 value, u64 threshold,
                           const char *fmt, ...)
{
    struct bottleneck_entry *e;
    va_list args;
    unsigned long flags;

    spin_lock_irqsave(&bn_lock, flags);

    if (bn_count >= MAX_BOTTLENECKS) {
        spin_unlock_irqrestore(&bn_lock, flags);
        return;
    }

    e = &bn_entries[bn_count];
    e->type = type;
    e->severity = severity;
    e->value = value;
    e->threshold = threshold;
    e->detected_time = ktime_get();

    va_start(args, fmt);
    vsnprintf(e->description, sizeof(e->description), fmt, args);
    va_end(args);

    bn_count++;

    spin_unlock_irqrestore(&bn_lock, flags);

    pr_info("smartmem-bottleneck: [%s] %s (severity=%s, value=%llu, threshold=%llu)\n",
            bottleneck_type_str(type), e->description,
            severity_str(severity), value, threshold);
}

/**
 * 检测慢分配瓶颈
 * 从热点数据中分析延迟情况
 */
static void detect_slow_alloc(void)
{
    struct hotspot_top_entry top[3];
    int count, i;
    u64 slow_count = 0;

    count = hotspot_get_top_n(top, 3);
    for (i = 0; i < count; i++) {
        if (top[i].latency_avg_ns > bn_thresholds.slow_alloc_ns)
            slow_count += top[i].alloc_count;
    }

    if (slow_count > bn_thresholds.slow_alloc_count) {
        enum bottleneck_severity sev;

        if (slow_count > bn_thresholds.slow_alloc_count * 10)
            sev = SEVERITY_CRITICAL;
        else if (slow_count > bn_thresholds.slow_alloc_count * 5)
            sev = SEVERITY_HIGH;
        else if (slow_count > bn_thresholds.slow_alloc_count * 2)
            sev = SEVERITY_MEDIUM;
        else
            sev = SEVERITY_LOW;

        add_bottleneck(BOTTLENECK_SLOW_ALLOC, sev,
                       slow_count, bn_thresholds.slow_alloc_count,
                       "Slow alloc detected: %llu events >1ms avg latency",
                       slow_count);
    }
}

/**
 * 检测内存碎片瓶颈
 * 检查高 order 页面可用性
 */
static void detect_fragmentation(void)
{
    int nid, z;
    struct zone *zone;
    unsigned long free_pages, free_order0 = 0, free_high = 0;

    for_each_online_node(nid) {
        pg_data_t *pgdat = NODE_DATA(nid);
        for (z = 0; z < MAX_NR_ZONES; z++) {
            zone = &pgdat->node_zones[z];
            if (zone->present_pages == 0)
                continue;

            free_pages = zone_page_state(zone, NR_FREE_PAGES);
            if (free_pages == 0)
                continue;

            free_order0 += zone->free_area[0].nr_free;
            free_high += zone->free_area[MAX_PAGE_ORDER - 1].nr_free;
        }
    }

    if (free_order0 > 0 && free_high == 0) {
        u64 frag_pct = 100;

        add_bottleneck(BOTTLENECK_HIGH_FRAGMENT,
                       frag_pct > 80 ? SEVERITY_HIGH : SEVERITY_MEDIUM,
                       frag_pct, bn_thresholds.fragment_percent,
                       "High fragmentation: no MAX_ORDER pages available, only order-0 free");
    }
}

/**
 * 检测 NUMA 不平衡瓶颈
 */
static void detect_numa_imbalance(void)
{
    int nid;
    int max_free = 0, min_free = 100;
    int num_nodes = 0;

    for_each_online_node(nid) {
        unsigned long free_pages, total_pages;
        int free_pct;

        total_pages = node_present_pages(nid);
        if (total_pages == 0)
            continue;

        num_nodes++;
        free_pages = 0;
        /* 简化：用全局 free 统计 */
        free_pct = (int)((global_zone_page_state(NR_FREE_PAGES) * 100) /
                         total_pages);
        if (free_pct > max_free)
            max_free = free_pct;
        if (free_pct < min_free)
            min_free = free_pct;
    }

    if (num_nodes >= 2) {
        int gap = max_free - min_free;
        if (gap > (int)bn_thresholds.numa_imbalance_percent) {
            add_bottleneck(BOTTLENECK_NUMA_IMBALANCE,
                           gap > 40 ? SEVERITY_HIGH : SEVERITY_MEDIUM,
                           gap, bn_thresholds.numa_imbalance_percent,
                           "NUMA imbalance: max_free=%d%%, min_free=%d%%, gap=%d%%",
                           max_free, min_free, gap);
        }
    }
}

/**
 * 检测 OOM 风险
 */
static void detect_oom_risk(void)
{
    unsigned long total_ram, free_ram;
    int free_pct;

    total_ram = totalram_pages();
    free_ram = global_zone_page_state(NR_FREE_PAGES);

    if (total_ram == 0)
        return;

    free_pct = (int)((free_ram * 100) / total_ram);

    if (free_pct < (int)bn_thresholds.oom_free_percent) {
        add_bottleneck(BOTTLENECK_OOM_RISK,
                       free_pct < 5 ? SEVERITY_CRITICAL : SEVERITY_HIGH,
                       free_pct, bn_thresholds.oom_free_percent,
                       "OOM risk: only %d%% free memory", free_pct);
    }
}

/**
 * 检测 SLAB 低命中率瓶颈
 * 比较 SLUB per-CPU 命中率和 miss 率
 */
static void detect_low_slab_hit(void)
{
    u64 alloc, free, hit, miss, tune;

    adaptive_slub_get_stats(&alloc, &free, &hit, &miss, &tune);

    if (alloc == 0)
        return;

    /* 命中率 = hit / (hit + miss) * 100 */
    {
        u64 total = hit + miss;
        u64 hit_pct = 100;

        if (total > 0)
            hit_pct = (hit * 100) / total;

        if (hit_pct < bn_thresholds.slab_hit_low_percent && alloc > 1000) {
            add_bottleneck(BOTTLENECK_LOW_SLAB_HIT,
                           hit_pct < 30 ? SEVERITY_HIGH : SEVERITY_MEDIUM,
                           hit_pct, bn_thresholds.slab_hit_low_percent,
                           "Low SLAB hit rate: %llu%% (alloc=%llu, hit=%llu, miss=%llu)",
                           hit_pct, alloc, hit, miss);
        }
    }
}

/**
 * 更新瓶颈分析 - 执行所有检测
 */
int bottleneck_update(void)
{
    unsigned long flags;

    if (!bottleneck_initialized)
        return -ENODEV;

    /* 清空旧结果 */
    spin_lock_irqsave(&bn_lock, flags);
    bn_count = 0;
    spin_unlock_irqrestore(&bn_lock, flags);

    /* 依次执行检测 */
    detect_slow_alloc();
    detect_fragmentation();
    detect_numa_imbalance();
    detect_oom_risk();
    detect_low_slab_hit();

    return 0;
}

/**
 * 获取瓶颈条目
 */
int bottleneck_get_entries(struct bottleneck_entry *entries, int max)
{
    unsigned long flags;
    int count, i;

    spin_lock_irqsave(&bn_lock, flags);
    count = min(bn_count, max);
    for (i = 0; i < count; i++)
        entries[i] = bn_entries[i];
    spin_unlock_irqrestore(&bn_lock, flags);

    return count;
}

/**
 * 设置瓶颈阈值
 */
void bottleneck_set_threshold(enum bottleneck_type type, u64 threshold)
{
    switch (type) {
    case BOTTLENECK_SLOW_ALLOC:
        bn_thresholds.slow_alloc_ns = threshold;
        break;
    case BOTTLENECK_HIGH_FRAGMENT:
        bn_thresholds.fragment_percent = threshold;
        break;
    case BOTTLENECK_NUMA_IMBALANCE:
        bn_thresholds.numa_imbalance_percent = threshold;
        break;
    case BOTTLENECK_LOW_SLAB_HIT:
        bn_thresholds.slab_hit_low_percent = threshold;
        break;
    case BOTTLENECK_OOM_RISK:
        bn_thresholds.oom_free_percent = threshold;
        break;
    default:
        break;
    }
}

u64 bottleneck_get_threshold(enum bottleneck_type type)
{
    switch (type) {
    case BOTTLENECK_SLOW_ALLOC:     return bn_thresholds.slow_alloc_ns;
    case BOTTLENECK_HIGH_FRAGMENT:  return bn_thresholds.fragment_percent;
    case BOTTLENECK_NUMA_IMBALANCE: return bn_thresholds.numa_imbalance_percent;
    case BOTTLENECK_LOW_SLAB_HIT:   return bn_thresholds.slab_hit_low_percent;
    case BOTTLENECK_OOM_RISK:       return bn_thresholds.oom_free_percent;
    default:                        return 0;
    }
}


/**
 * 瓶颈分析初始化
 */
int bottleneck_init(void)
{
    pr_info("smartmem: bottleneck analysis initializing...\n");

    bn_count = 0;
    bottleneck_initialized = true;

    pr_info("smartmem: bottleneck analysis initialized\n");
    return 0;
}

/**
 * 瓶颈分析退出
 */
void bottleneck_exit(void)
{
    pr_info("smartmem: bottleneck analysis exiting...\n");

    bn_count = 0;
    bottleneck_initialized = false;

    pr_info("smartmem: bottleneck analysis exited\n");
}