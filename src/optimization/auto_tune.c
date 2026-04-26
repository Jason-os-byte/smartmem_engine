/* 自动调优实现 */
#include "auto_tune.h"
#include "bottleneck.h"
#include "root_cause.h"
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/swap.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/fs.h>
#include <linux/sysctl.h>

static bool auto_tune_initialized = false;
static bool auto_tune_running = false;

/* 调优统计 */
static struct tune_stats at_stats;

/* 调优历史 */
static struct tune_history at_history[MAX_TUNE_HISTORY];
static int at_history_count = 0;
static int at_history_idx = 0;  /* 环形缓冲区写位置 */
static DEFINE_SPINLOCK(at_history_lock);

/* 定期调优工作队列 */
static struct delayed_work at_work;
#define AT_WORK_INTERVAL_MS  10000  /* 10秒检查一次 */

/* 调优冷却：同一动作至少间隔 30 秒 */
#define AT_COOLDOWN_SEC  30
static ktime_t at_last_compact;
static ktime_t at_last_watermark;
static ktime_t at_last_numa;
static ktime_t at_last_slab;

static bool cooldown_ok(ktime_t last_time)
{
    ktime_t diff = ktime_sub(ktime_get(), last_time);
    return ktime_to_ns(diff) > (s64)AT_COOLDOWN_SEC * NSEC_PER_SEC;
}

/**
 * 记录调优历史
 */
static void record_history(enum tune_action action,
                           enum bottleneck_type trigger,
                           int result, const char *desc)
{
    struct tune_history *h;
    unsigned long flags;

    spin_lock_irqsave(&at_history_lock, flags);

    h = &at_history[at_history_idx];
    h->action = action;
    h->trigger = trigger;
    h->timestamp = ktime_get();
    h->result = result;
    strscpy(h->description, desc, sizeof(h->description));

    at_history_idx = (at_history_idx + 1) % MAX_TUNE_HISTORY;
    if (at_history_count < MAX_TUNE_HISTORY)
        at_history_count++;

    spin_unlock_irqrestore(&at_history_lock, flags);
}

/**
 * 获取当前空闲内存百分比
 */
static int get_free_percent(void)
{
    unsigned long total_ram = totalram_pages();
    if (total_ram == 0)
        return 0;
    return (int)((global_zone_page_state(NR_FREE_PAGES) * 100) / total_ram);
}

/**
 * 触发内存整理（真正执行）
 * 写 /proc/sys/vm/compact_memory = 1 触发全系统 compaction
 */
static int do_compact(enum bottleneck_type trigger)
{
    int free_before, free_after;
    char desc[64];

    if (!cooldown_ok(at_last_compact))
        return -EAGAIN;

    free_before = get_free_percent();

    pr_info("smartmem-autotune: triggering memory compaction (free=%d%%)\n",
            free_before);

    /* 写 compact_memory=1 触发内核同步整理 */
    {
        struct file *filp;
        loff_t pos = 0;
        char buf[] = "1";
        ssize_t written;

        filp = filp_open("/proc/sys/vm/compact_memory",
                         O_WRONLY, 0);
        if (!IS_ERR(filp)) {
            written = kernel_write(filp, buf, 1, &pos);
            filp_close(filp, NULL);
            if (written != 1) {
                pr_warn("smartmem-autotune: compact_memory write failed\n");
                atomic64_inc(&at_stats.fail_count);
                record_history(TUNE_ACTION_COMPACT, trigger, -EIO,
                               "Compaction write failed");
                at_last_compact = ktime_get();
                return -EIO;
            }
        } else {
            pr_warn("smartmem-autotune: cannot open compact_memory\n");
            atomic64_inc(&at_stats.fail_count);
            at_last_compact = ktime_get();
            return -EIO;
        }
    }

    free_after = get_free_percent();

    at_last_compact = ktime_get();
    atomic64_inc(&at_stats.compact_count);
    atomic64_inc(&at_stats.total_tune_count);

    snprintf(desc, sizeof(desc), "Compaction: %d%% -> %d%% free",
             free_before, free_after);
    record_history(TUNE_ACTION_COMPACT, trigger, 0, desc);

    pr_info("smartmem-autotune: compaction done (%d%% -> %d%%)\n",
            free_before, free_after);
    return 0;
}

/**
 * 调整 min_free_kbytes 水位线（真正执行）
 * 当空闲率低时，增加 min_free_kbytes 让内核更早开始回收
 */
static int do_adjust_watermark(enum bottleneck_type trigger)
{
    int free_pct;
    int cur_mfk, new_mfk;
    char desc[64];
    char buf[16];

    if (!cooldown_ok(at_last_watermark))
        return -EAGAIN;

    free_pct = get_free_percent();
    if (free_pct >= 10)
        return 0;

    /* 读取当前 min_free_kbytes */
    {
        struct file *filp;
        loff_t pos = 0;
        ssize_t n;

        filp = filp_open("/proc/sys/vm/min_free_kbytes", O_RDONLY, 0);
        if (IS_ERR(filp))
            return -EIO;

        n = kernel_read(filp, buf, sizeof(buf) - 1, &pos);
        filp_close(filp, NULL);
        if (n <= 0)
            return -EIO;

        buf[n] = '\0';
        if (kstrtoint(buf, 10, &cur_mfk))
            return -EINVAL;
    }

    /* 增加 20% */
    new_mfk = cur_mfk + (cur_mfk / 5);

    pr_info("smartmem-autotune: adjusting min_free_kbytes %d -> %d "
            "(free=%d%%)\n", cur_mfk, new_mfk, free_pct);

    /* 写入新值 */
    {
        struct file *filp;
        loff_t pos = 0;
        ssize_t written;

        snprintf(buf, sizeof(buf), "%d", new_mfk);

        filp = filp_open("/proc/sys/vm/min_free_kbytes", O_WRONLY, 0);
        if (!IS_ERR(filp)) {
            written = kernel_write(filp, buf, strlen(buf), &pos);
            filp_close(filp, NULL);
            if (written <= 0) {
                pr_warn("smartmem-autotune: min_free_kbytes write failed\n");
                atomic64_inc(&at_stats.fail_count);
                at_last_watermark = ktime_get();
                return -EIO;
            }
        } else {
            atomic64_inc(&at_stats.fail_count);
            at_last_watermark = ktime_get();
            return -EIO;
        }
    }

    at_last_watermark = ktime_get();
    atomic64_inc(&at_stats.watermark_count);
    atomic64_inc(&at_stats.total_tune_count);

    snprintf(desc, sizeof(desc), "min_free_kbytes: %d -> %d (free=%d%%)",
             cur_mfk, new_mfk, free_pct);
    record_history(TUNE_ACTION_ADJUST_WATERMARK, trigger, 0, desc);

    return 0;
}

/**
 * 释放页缓存回收内存（真正执行）
 * 写 /proc/sys/vm/drop_caches = 1 释放页缓存
 */
static int do_numa_migrate(enum bottleneck_type trigger)
{
    int free_before, free_after;
    char desc[64];

    if (!cooldown_ok(at_last_numa))
        return -EAGAIN;

    free_before = get_free_percent();

    pr_info("smartmem-autotune: dropping page caches (free=%d%%)\n",
            free_before);

    /* drop_caches=1 释放页缓存 */
    {
        struct file *filp;
        loff_t pos = 0;
        char buf[] = "1";
        ssize_t written;

        filp = filp_open("/proc/sys/vm/drop_caches", O_WRONLY, 0);
        if (!IS_ERR(filp)) {
            written = kernel_write(filp, buf, 1, &pos);
            filp_close(filp, NULL);
            if (written != 1) {
                pr_warn("smartmem-autotune: drop_caches write failed\n");
                atomic64_inc(&at_stats.fail_count);
                at_last_numa = ktime_get();
                return -EIO;
            }
        } else {
            atomic64_inc(&at_stats.fail_count);
            at_last_numa = ktime_get();
            return -EIO;
        }
    }

    free_after = get_free_percent();

    at_last_numa = ktime_get();
    atomic64_inc(&at_stats.numa_migrate_count);
    atomic64_inc(&at_stats.total_tune_count);

    snprintf(desc, sizeof(desc), "Drop caches: %d%% -> %d%% free",
             free_before, free_after);
    record_history(TUNE_ACTION_NUMA_MIGRATE, trigger, 0, desc);

    pr_info("smartmem-autotune: drop caches done (%d%% -> %d%%)\n",
            free_before, free_after);
    return 0;
}

/**
 * 释放 SLAB 可回收对象（真正执行）
 * 写 /proc/sys/vm/drop_caches = 2 释放 slab 可回收对象
 */
static int do_adjust_slab(enum bottleneck_type trigger)
{
    int free_before, free_after;
    char desc[64];

    if (!cooldown_ok(at_last_slab))
        return -EAGAIN;

    free_before = get_free_percent();

    pr_info("smartmem-autotune: dropping slab caches (free=%d%%)\n",
            free_before);

    /* drop_caches=2 释放 slab 可回收对象 */
    {
        struct file *filp;
        loff_t pos = 0;
        char buf[] = "2";
        ssize_t written;

        filp = filp_open("/proc/sys/vm/drop_caches", O_WRONLY, 0);
        if (!IS_ERR(filp)) {
            written = kernel_write(filp, buf, 1, &pos);
            filp_close(filp, NULL);
            if (written != 1) {
                pr_warn("smartmem-autotune: drop_caches(2) write failed\n");
                atomic64_inc(&at_stats.fail_count);
                at_last_slab = ktime_get();
                return -EIO;
            }
        } else {
            atomic64_inc(&at_stats.fail_count);
            at_last_slab = ktime_get();
            return -EIO;
        }
    }

    free_after = get_free_percent();

    at_last_slab = ktime_get();
    atomic64_inc(&at_stats.slab_adjust_count);
    atomic64_inc(&at_stats.total_tune_count);

    snprintf(desc, sizeof(desc), "Drop slab: %d%% -> %d%% free",
             free_before, free_after);
    record_history(TUNE_ACTION_ADJUST_SLAB, trigger, 0, desc);

    pr_info("smartmem-autotune: drop slab done (%d%% -> %d%%)\n",
            free_before, free_after);
    return 0;
}

/**
 * 根据瓶颈类型选择调优动作
 */
static int select_tune_action(struct bottleneck_entry *bottlenecks,
                               int bn_cnt)
{
    int i;
    int actions_taken = 0;

    for (i = 0; i < bn_cnt; i++) {
        switch (bottlenecks[i].type) {
        case BOTTLENECK_HIGH_FRAGMENT:
            if (do_compact(BOTTLENECK_HIGH_FRAGMENT) == 0)
                actions_taken++;
            break;

        case BOTTLENECK_OOM_RISK:
            if (do_compact(BOTTLENECK_OOM_RISK) == 0)
                actions_taken++;
            if (do_adjust_watermark(BOTTLENECK_OOM_RISK) == 0)
                actions_taken++;
            break;

        case BOTTLENECK_SLOW_ALLOC:
            if (do_adjust_watermark(BOTTLENECK_SLOW_ALLOC) == 0)
                actions_taken++;
            break;

        case BOTTLENECK_NUMA_IMBALANCE:
            if (do_numa_migrate(BOTTLENECK_NUMA_IMBALANCE) == 0)
                actions_taken++;
            break;

        case BOTTLENECK_LOW_SLAB_HIT:
            if (do_adjust_slab(BOTTLENECK_LOW_SLAB_HIT) == 0)
                actions_taken++;
            break;

        default:
            break;
        }
    }

    return actions_taken;
}

/**
 * 定期调优工作函数
 */
static void auto_tune_work_fn(struct work_struct *work)
{
    struct bottleneck_entry bottlenecks[MAX_BOTTLENECKS];
    int bn_cnt;

    if (!auto_tune_running)
        return;

    /* 先更新瓶颈分析 */
    bottleneck_update();

    /* 获取瓶颈 */
    bn_cnt = bottleneck_get_entries(bottlenecks, MAX_BOTTLENECKS);

    if (bn_cnt > 0) {
        pr_info("smartmem-autotune: detected %d bottleneck(s), tuning...\n",
                bn_cnt);
        select_tune_action(bottlenecks, bn_cnt);
    }

    /* 重新调度 */
    if (auto_tune_running)
        schedule_delayed_work(&at_work,
                              msecs_to_jiffies(AT_WORK_INTERVAL_MS));
}

/**
 * 执行调优检查（手动触发）
 */
int auto_tune_check(void)
{
    struct bottleneck_entry bottlenecks[MAX_BOTTLENECKS];
    int bn_cnt;

    if (!auto_tune_initialized)
        return -ENODEV;

    bottleneck_update();
    bn_cnt = bottleneck_get_entries(bottlenecks, MAX_BOTTLENECKS);

    if (bn_cnt == 0)
        return 0;

    return select_tune_action(bottlenecks, bn_cnt);
}

/**
 * 手动触发指定调优动作
 */
int auto_tune_trigger(enum tune_action action)
{
    if (!auto_tune_initialized)
        return -ENODEV;

    switch (action) {
    case TUNE_ACTION_COMPACT:
        return do_compact(BOTTLENECK_HIGH_FRAGMENT);
    case TUNE_ACTION_ADJUST_WATERMARK:
        return do_adjust_watermark(BOTTLENECK_OOM_RISK);
    case TUNE_ACTION_NUMA_MIGRATE:
        return do_numa_migrate(BOTTLENECK_NUMA_IMBALANCE);
    case TUNE_ACTION_ADJUST_SLAB:
        return do_adjust_slab(BOTTLENECK_LOW_SLAB_HIT);
    default:
        return -EINVAL;
    }
}

/**
 * 获取调优统计
 */
void auto_tune_get_stats(struct tune_stats *stats)
{
    if (stats)
        *stats = at_stats;
}

/**
 * 获取调优历史
 */
int auto_tune_get_history(struct tune_history *entries, int max)
{
    unsigned long flags;
    int count, i, idx;

    spin_lock_irqsave(&at_history_lock, flags);
    count = min(at_history_count, max);

    /* 从最新到最旧输出 */
    for (i = 0; i < count; i++) {
        idx = (at_history_idx - 1 - i + MAX_TUNE_HISTORY) %
              MAX_TUNE_HISTORY;
        entries[i] = at_history[idx];
    }
    spin_unlock_irqrestore(&at_history_lock, flags);

    return count;
}

/**
 * 重置统计
 */
void auto_tune_reset_stats(void)
{
    unsigned long flags;

    atomic64_set(&at_stats.total_tune_count, 0);
    atomic64_set(&at_stats.compact_count, 0);
    atomic64_set(&at_stats.watermark_count, 0);
    atomic64_set(&at_stats.numa_migrate_count, 0);
    atomic64_set(&at_stats.slab_adjust_count, 0);
    atomic64_set(&at_stats.fail_count, 0);

    spin_lock_irqsave(&at_history_lock, flags);
    at_history_count = 0;
    at_history_idx = 0;
    spin_unlock_irqrestore(&at_history_lock, flags);
}

/**
 * 自动调优初始化
 */
int auto_tune_init(void)
{
    pr_info("smartmem: auto tune initializing...\n");

    auto_tune_reset_stats();

    INIT_DELAYED_WORK(&at_work, auto_tune_work_fn);

    auto_tune_initialized = true;

    pr_info("smartmem: auto tune initialized\n");
    return 0;
}

/**
 * 自动调优退出
 */
void auto_tune_exit(void)
{
    pr_info("smartmem: auto tune exiting...\n");

    if (auto_tune_running)
        auto_tune_stop();

    auto_tune_initialized = false;

    pr_info("smartmem: auto tune exited\n");
}

/**
 * 启动自动调优
 */
int auto_tune_start(void)
{
    pr_info("smartmem: starting auto tune...\n");

    if (!auto_tune_initialized)
        return -ENODEV;

    auto_tune_running = true;

    /* 启动定期检查 */
    schedule_delayed_work(&at_work,
                          msecs_to_jiffies(AT_WORK_INTERVAL_MS));

    pr_info("smartmem: auto tune started (interval=%dms)\n",
            AT_WORK_INTERVAL_MS);
    return 0;
}

/**
 * 停止自动调优
 */
int auto_tune_stop(void)
{
    pr_info("smartmem: stopping auto tune...\n");

    auto_tune_running = false;
    cancel_delayed_work_sync(&at_work);

    pr_info("smartmem: auto tune stopped (total_tunes=%llu)\n",
            atomic64_read(&at_stats.total_tune_count));
    return 0;
}
