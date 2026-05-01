/* debugfs接口实现 - 调试和内部数据导出 */

#include "debugfs.h"
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include "config.h"
#include "stats.h"
#include "strategy.h"
#include "hotspot.h"
#include "bottleneck.h"
#include "root_cause.h"
#include "auto_tune.h"
#include "predictive.h"
#include "trace_monitor.h"
#include "ebpf_monitor.h"

static struct dentry *smartmem_debug_dir = NULL;

/**
 * 状态dump - 完整内部状态
 */
static int status_show(struct seq_file *m, void *v)
{
    char value[32];

    seq_printf(m, "=== SmartMemEngine Full State Dump ===\n\n");

    /* 全局状态 */
    seq_printf(m, "[Global]\n");
    seq_printf(m, "  version:          1.0.0\n");
    seq_printf(m, "  ebpf_active:      %s\n",
               ebpf_monitor_is_active() ? "yes" : "no");
    seq_printf(m, "  trace_running:    events=%llu allocs=%llu frees=%llu\n",
               trace_monitor_get_event_count(),
               trace_monitor_get_alloc_count(),
               trace_monitor_get_free_count());
    seq_printf(m, "\n");

    /* 配置状态 */
    seq_printf(m, "[Configuration]\n");
    if (smartmem_config_get("hook_buddy_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "  hook_buddy_enabled:    %s\n", value);
    if (smartmem_config_get("hook_slub_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "  hook_slub_enabled:     %s\n", value);
    if (smartmem_config_get("hook_vma_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "  hook_vma_enabled:      %s\n", value);
    if (smartmem_config_get("hook_lru_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "  hook_lru_enabled:      %s\n", value);
    if (smartmem_config_get("hook_numa_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "  hook_numa_enabled:     %s\n", value);
    if (smartmem_config_get("auto_tune_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "  auto_tune_enabled:     %s\n", value);
    if (smartmem_config_get("trace_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "  trace_enabled:         %s\n", value);
    seq_printf(m, "\n");

    /* Hook 统计 */
    seq_printf(m, "[Hook Stats]\n");
    seq_printf(m, "  buddy_alloc:      %llu\n", smartmem_stats_get_buddy_alloc());
    seq_printf(m, "  buddy_free:       %llu\n", smartmem_stats_get_buddy_free());
    seq_printf(m, "  slub_alloc:       %llu\n", smartmem_stats_get_slub_alloc());
    seq_printf(m, "  slub_free:        %llu\n", smartmem_stats_get_slub_free());
    seq_printf(m, "  numa_local:       %llu\n", smartmem_stats_get_numa_local());
    seq_printf(m, "  numa_remote:      %llu\n", smartmem_stats_get_numa_remote());
    seq_printf(m, "\n");

    /* 策略状态 */
    seq_printf(m, "[Strategies]\n");
    {
        struct buddy_strategy *bs = buddy_strategy_get_current();
        struct slub_strategy *ss = slub_strategy_get_current();
        struct lru_strategy *ls = lru_strategy_get_current();
        struct numa_strategy *ns = numa_strategy_get_current();

        seq_printf(m, "  buddy:            %s\n", bs ? bs->name : "none");
        seq_printf(m, "  slub:             %s\n", ss ? ss->name : "none");
        seq_printf(m, "  lru:              %s\n", ls ? ls->name : "none");
        seq_printf(m, "  numa:             %s\n", ns ? ns->name : "none");
    }
    seq_printf(m, "\n");

    /* 瓶颈分析结果 */
    seq_printf(m, "[Bottlenecks]\n");
    {
        struct bottleneck_entry entries[MAX_BOTTLENECKS];
        int count, i;

        bottleneck_update();
        count = bottleneck_get_entries(entries, MAX_BOTTLENECKS);
        if (count == 0) {
            seq_printf(m, "  none\n");
        } else {
            for (i = 0; i < count; i++) {
                seq_printf(m, "  [%d] type=%d severity=%d value=%llu/%llu %s\n",
                           i, entries[i].type, entries[i].severity,
                           entries[i].value, entries[i].threshold,
                           entries[i].description);
            }
        }
    }
    seq_printf(m, "\n");

    /* 根因分析结果 */
    seq_printf(m, "[Root Causes]\n");
    {
        struct root_cause_entry entries[MAX_ROOT_CAUSES];
        int count, i;

        root_cause_update();
        count = root_cause_get_entries(entries, MAX_ROOT_CAUSES);
        if (count == 0) {
            seq_printf(m, "  none\n");
        } else {
            for (i = 0; i < count; i++) {
                seq_printf(m, "  [%d] type=%d confidence=%llu%% %s\n",
                           i, entries[i].type, entries[i].confidence,
                           entries[i].description);
            }
        }
    }
    seq_printf(m, "\n");

    /* 调优状态 */
    seq_printf(m, "[Auto-Tune]\n");
    {
        struct tune_stats ts;
        auto_tune_get_stats(&ts);
        seq_printf(m, "  total_tunes:      %llu\n", atomic64_read(&ts.total_tune_count));
        seq_printf(m, "  compact:          %llu\n", atomic64_read(&ts.compact_count));
        seq_printf(m, "  watermark:        %llu\n", atomic64_read(&ts.watermark_count));
        seq_printf(m, "  numa_migrate:     %llu\n", atomic64_read(&ts.numa_migrate_count));
        seq_printf(m, "  slab_adjust:      %llu\n", atomic64_read(&ts.slab_adjust_count));
        seq_printf(m, "  failures:         %llu\n", atomic64_read(&ts.fail_count));
    }
    seq_printf(m, "\n");

    /* 预测状态 */
    seq_printf(m, "[Prediction]\n");
    {
        struct predict_stats ps;
        predictive_get_stats(&ps);
        seq_printf(m, "  samples:          %llu\n", atomic64_read(&ps.sample_count));
        seq_printf(m, "  predictions:      %llu\n", atomic64_read(&ps.prediction_count));
        seq_printf(m, "  oom_predicts:     %llu\n", atomic64_read(&ps.oom_predict_count));
    }

    return 0;
}

static int status_open(struct inode *inode, struct file *file)
{
    return single_open(file, status_show, NULL);
}

static const struct file_operations status_fops = {
    .owner = THIS_MODULE,
    .open = status_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/**
 * 热点详细数据导出
 * 比 procfs 版本输出更多内部细节
 */
static int hotspots_debug_show(struct seq_file *m, void *v)
{
    struct hotspot_top_entry *top;
    int count, i;

    top = kmalloc_array(HOTSPOT_TOP_N, sizeof(struct hotspot_top_entry),
                        GFP_KERNEL);
    if (!top)
        return -ENOMEM;

    seq_printf(m, "=== Hotspot Detail Dump ===\n\n");

    count = hotspot_get_top_n(top, HOTSPOT_TOP_N);
    if (count <= 0) {
        seq_printf(m, "No hotspot data.\n");
        kfree(top);
        return 0;
    }

    for (i = 0; i < count; i++) {
        seq_printf(m, "--- Hotspot #%d ---\n", i + 1);
        seq_printf(m, "  stack_hash:     0x%08x\n", top[i].stack_hash);
        seq_printf(m, "  score:          %llu\n", top[i].score);
        seq_printf(m, "  alloc_count:    %llu\n", top[i].alloc_count);
        seq_printf(m, "  alloc_pages:    %llu\n", top[i].alloc_pages);
        seq_printf(m, "  latency_avg:    %lluns\n", top[i].latency_avg_ns);
        seq_printf(m, "  latency_max:    %lluns\n", top[i].latency_max_ns);
        seq_printf(m, "  stack_depth:    %d\n", top[i].stack_depth);

        if (top[i].stack_depth > 0) {
            int j;
            seq_printf(m, "  raw_stack:\n");
            for (j = 0; j < top[i].stack_depth && j < HOTSPOT_STACK_DEPTH; j++) {
                if (top[i].stack_frames[j] == 0)
                    break;
                seq_printf(m, "    [%d] 0x%016lx\n", j, top[i].stack_frames[j]);
            }
            seq_printf(m, "  resolved_stack:\n");
            for (j = 0; j < top[i].stack_depth && j < HOTSPOT_STACK_DEPTH; j++) {
                char sym[KSYM_SYMBOL_LEN];
                if (top[i].stack_frames[j] == 0)
                    break;
                sprint_symbol(sym, top[i].stack_frames[j]);
                seq_printf(m, "    [%d] %s\n", j, sym);
            }
        }
        seq_printf(m, "\n");
    }

    kfree(top);
    return 0;
}

static int hotspots_debug_open(struct inode *inode, struct file *file)
{
    return single_open(file, hotspots_debug_show, NULL);
}

static const struct file_operations hotspots_debug_fops = {
    .owner = THIS_MODULE,
    .open = hotspots_debug_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/**
 * 调优历史详细导出
 */
static int tune_history_show(struct seq_file *m, void *v)
{
    struct tune_history *history;
    int count, i;

    history = kmalloc_array(MAX_TUNE_HISTORY, sizeof(struct tune_history),
                            GFP_KERNEL);
    if (!history)
        return -ENOMEM;

    seq_printf(m, "=== Tune History Dump ===\n\n");

    count = auto_tune_get_history(history, MAX_TUNE_HISTORY);
    if (count <= 0) {
        seq_printf(m, "No tune history.\n");
        kfree(history);
        return 0;
    }

    for (i = 0; i < count; i++) {
        static const char *action_str[] = {
            "none", "compact", "adjust_watermark",
            "numa_migrate", "adjust_slab"
        };
        u64 ts_ns, ts_sec;

        ts_ns = ktime_to_ns(history[i].timestamp);
        ts_sec = ts_ns;
        do_div(ts_sec, NSEC_PER_SEC);

        seq_printf(m, "[%d] time=%llu action=%-16s trigger=%d result=%d\n",
                   i + 1, ts_sec,
                   history[i].action < 5 ? action_str[history[i].action] : "unknown",
                   history[i].trigger, history[i].result);
        seq_printf(m, "     %s\n", history[i].description);
    }

    kfree(history);
    return 0;
}

static int tune_history_open(struct inode *inode, struct file *file)
{
    return single_open(file, tune_history_show, NULL);
}

static const struct file_operations tune_history_fops = {
    .owner = THIS_MODULE,
    .open = tune_history_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/**
 * debugfs初始化
 */
int smartmem_debugfs_init(void)
{
    pr_info("smartmem: debugfs initializing...\n");

    smartmem_debug_dir = debugfs_create_dir("smartmem", NULL);
    if (IS_ERR(smartmem_debug_dir)) {
        pr_err("smartmem: failed to create debugfs dir\n");
        return -ENOMEM;
    }

    /* /sys/kernel/debug/smartmem/status - 完整状态dump */
    debugfs_create_file("status", 0444, smartmem_debug_dir, NULL,
                        &status_fops);

    /* /sys/kernel/debug/smartmem/hotspots - 热点详细数据 */
    debugfs_create_file("hotspots", 0444, smartmem_debug_dir, NULL,
                        &hotspots_debug_fops);

    /* /sys/kernel/debug/smartmem/tune_history - 调优历史 */
    debugfs_create_file("tune_history", 0444, smartmem_debug_dir, NULL,
                        &tune_history_fops);

    pr_info("smartmem: debugfs initialized (/sys/kernel/debug/smartmem/)\n");
    return 0;
}

/**
 * debugfs退出
 */
void smartmem_debugfs_exit(void)
{
    pr_info("smartmem: debugfs exiting...\n");

    debugfs_remove_recursive(smartmem_debug_dir);
    smartmem_debug_dir = NULL;

    pr_info("smartmem: debugfs exited\n");
}