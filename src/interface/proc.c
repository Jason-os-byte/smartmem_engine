/* procfs接口实现 */

#include "proc.h"
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include "config.h"
#include "strategy.h"
#include "stats.h"
#include "trace_monitor.h"
#include "ebpf_monitor.h"
#include "analysis.h"
#include "hotspot.h"
#include "bottleneck.h"
#include "root_cause.h"
#include "auto_tune.h"
#include "predictive.h"
#include "numa_buddy.h"
#include "adaptive_slub.h"
#include "multi_gen_lru.h"
#include "numa_balance.h"
#include "monitor.h"


static struct proc_dir_entry *smartmem_dir = NULL;

/**
 * 显示配置
 */
static int config_show(struct seq_file *m, void *v)
{
    char value[32];

    seq_printf(m, "SmartMemEngine Configuration\n");
    seq_printf(m, "============================\n");
    seq_printf(m, "Version: 1.0.0\n");
    seq_printf(m, "Status: Running\n");

    if (smartmem_config_get("hook_buddy_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "hook_buddy_enabled=%s\n", value);
    if (smartmem_config_get("hook_slub_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "hook_slub_enabled=%s\n", value);
    if (smartmem_config_get("hook_vma_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "hook_vma_enabled=%s\n", value);
    if (smartmem_config_get("hook_lru_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "hook_lru_enabled=%s\n", value);
    if (smartmem_config_get("hook_numa_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "hook_numa_enabled=%s\n", value);
    if (smartmem_config_get("numa_aware_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "numa_aware_enabled=%s\n", value);
    if (smartmem_config_get("adaptive_slub_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "adaptive_slub_enabled=%s\n", value);
    if (smartmem_config_get("ebpf_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "ebpf_enabled=%s\n", value);
    if (smartmem_config_get("trace_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "trace_enabled=%s\n", value);
    if (smartmem_config_get("auto_tune_enabled", value, sizeof(value)) == 0)
        seq_printf(m, "auto_tune_enabled=%s\n", value);


    return 0;
}

/**
 * 打开配置文件
 */
static int config_open(struct inode *inode, struct file *file)
{
    return single_open(file, config_show, NULL);
}

/**
 * 写入配置文件
 */
static ssize_t config_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos) 
{
    char *buf, *key, *value;
    int ret;

    if (count > 256) {
        return -EINVAL;
    }

    buf = kmalloc(count + 1, GFP_KERNEL);
    if (!buf) {
        return -ENOMEM;
    }

    if (copy_from_user(buf, buffer, count)) {
        kfree(buf);
        return -EFAULT;
    }

    buf[count] = '\0';

    /* 解析 key=value 格式 */
    key = buf;
    value = strchr(buf, '=');
    if (!value) {
        kfree(buf);
        return -EINVAL;
    }
    *value++ = '\0';

    /* 去除换行符 */
    if (value[strlen(value) - 1] == '\n') {
        value[strlen(value) - 1] = '\0';
    }

    ret = smartmem_config_set(key, value);
    kfree(buf);

    if (ret) {
        return ret;
    }

    return count;
}

static const struct proc_ops config_proc_ops = {
    .proc_open = config_open,
    .proc_read = seq_read,
    .proc_write = config_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/**
 * 显示策略信息
 */
static int policies_show(struct seq_file *m, void *v)
{
    struct buddy_strategy *buddy_stra;
    struct slub_strategy *slub_stra;
    struct lru_strategy *ls;
    struct numa_strategy *ns;

    seq_printf(m, "SmartMemEngine Policies\n");
    seq_printf(m, "=======================\n\n");

    /* Buddy 策略 */
    seq_printf(m, "Buddy Strategy:\n");
    buddy_stra = buddy_strategy_get_current();
    if (buddy_stra) {
        u64 local, remote, forced;
        seq_printf(m, "  Name:        %s\n", buddy_stra->name);
        seq_printf(m, "  Version:     %s\n", buddy_stra->version);
        seq_printf(m, "  Description: %s\n", buddy_stra->description);
        seq_printf(m, "  Enabled:     yes\n");
        numa_buddy_get_stats(&local, &remote, &forced);
        seq_printf(m, "  Statistics:\n");
        seq_printf(m, "    local_alloc:   %llu\n", local);
        seq_printf(m, "    remote_alloc:  %llu\n", remote);
        seq_printf(m, "    forced_alloc:  %llu\n", forced);
    } else {
        seq_printf(m, "  No active strategy\n");
    }

    /* SLUB 策略 */
    seq_printf(m, "\nSLUB Strategy:\n");
    slub_stra = slub_strategy_get_current();
    if (slub_stra) {
        u64 alloc, free, hit, miss, tune;
        seq_printf(m, "  Name:        %s\n", slub_stra->name);
        seq_printf(m, "  Version:     %s\n", slub_stra->version);
        seq_printf(m, "  Description: %s\n", slub_stra->description);
        seq_printf(m, "  Enabled:     yes\n");
        adaptive_slub_get_stats(&alloc, &free, &hit, &miss, &tune);
        seq_printf(m, "  Statistics:\n");
        seq_printf(m, "    alloc_count:   %llu\n", alloc);
        seq_printf(m, "    free_count:    %llu\n", free);
        seq_printf(m, "    percpu_hit:    %llu\n", hit);
        seq_printf(m, "    percpu_miss:   %llu\n", miss);
        seq_printf(m, "    tune_count:    %llu\n", tune);
    } else {
        seq_printf(m, "  No active strategy\n");
    }

    /* LRU 策略 */
    seq_printf(m, "\nLRU Strategy:\n");
    ls = lru_strategy_get_current();
    if (ls) {
        u64 young_acc, old_acc, promote, demote, evict;
        seq_printf(m, "  Name:        %s\n", ls->name);
        seq_printf(m, "  Version:     %s\n", ls->version);
        seq_printf(m, "  Description: %s\n", ls->description);
        seq_printf(m, "  Enabled:     yes\n");
        mglru_get_stats(&young_acc, &old_acc, &promote, &demote, &evict);
        seq_printf(m, "  Statistics:\n");
        seq_printf(m, "    young_accessed: %llu\n", young_acc);
        seq_printf(m, "    old_accessed:   %llu\n", old_acc);
        seq_printf(m, "    promote_count:  %llu\n", promote);
        seq_printf(m, "    demote_count:   %llu\n", demote);
        seq_printf(m, "    evict_count:    %llu\n", evict);
    } else {
        seq_printf(m, "  No active strategy\n");
    }

    /* NUMA 策略 */
    seq_printf(m, "\nNUMA Strategy:\n");
    ns = numa_strategy_get_current();
    if (ns) {
        u64 migrations, imbalance_det, balance;
        seq_printf(m, "  Name:        %s\n", ns->name);
        seq_printf(m, "  Version:     %s\n", ns->version);
        seq_printf(m, "  Description: %s\n", ns->description);
        seq_printf(m, "  Enabled:     yes\n");
        numa_balance_get_stats(&migrations, &imbalance_det, &balance);
        seq_printf(m, "  Statistics:\n");
        seq_printf(m, "    migrations:         %llu\n", migrations);
        seq_printf(m, "    imbalance_detects:  %llu\n", imbalance_det);
        seq_printf(m, "    balance_actions:    %llu\n", balance);
    } else {
        seq_printf(m, "  No active strategy\n");
    }

    return 0;
}

static int policies_open(struct inode *inode, struct file *file) 
{
    return single_open(file, policies_show, NULL);
}

static const struct proc_ops policies_proc_ops = {
    .proc_open = policies_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/**
 * 显示统计信息
 */
static int stats_show(struct seq_file *m, void *v)
{
    seq_printf(m, "SmartMemEngine Statistics\n");
    seq_printf(m, "=========================\n\n");

    seq_printf(m, "Buddy Allocator:\n");
    seq_printf(m, "  alloc_count: %llu\n", smartmem_stats_get_buddy_alloc());
    seq_printf(m, "  free_count:  %llu\n", smartmem_stats_get_buddy_free());
    seq_printf(m, "\n");

    seq_printf(m, "SLUB Allocator:\n");
    seq_printf(m, "  alloc_count: %llu\n", smartmem_stats_get_slub_alloc());
    seq_printf(m, "  free_count:  %llu\n", smartmem_stats_get_slub_free());
    seq_printf(m, "\n");

    seq_printf(m, "NUMA:\n");
    seq_printf(m, "  local_alloc:  %llu\n", smartmem_stats_get_numa_local());
    seq_printf(m, "  remote_alloc: %llu\n", smartmem_stats_get_numa_remote());
    seq_printf(m, "\n");

    seq_printf(m, "Monitoring:\n");
    seq_printf(m, "  ebpf_active:    %s\n", ebpf_monitor_is_active() ? "yes" : "no");
    seq_printf(m, "  trace_events:   %llu\n", trace_monitor_get_event_count());
    seq_printf(m, "  trace_allocs:   %llu\n", trace_monitor_get_alloc_count());
    seq_printf(m, "  trace_frees:    %llu\n", trace_monitor_get_free_count());
    seq_printf(m, "\n");

    seq_printf(m, "Auto-Tune:\n");
    {
        struct tune_stats ts;
        auto_tune_get_stats(&ts);
        seq_printf(m, "  total_tunes:    %llu\n", atomic64_read(&ts.total_tune_count));
        seq_printf(m, "  compact:        %llu\n", atomic64_read(&ts.compact_count));
        seq_printf(m, "  failures:       %llu\n", atomic64_read(&ts.fail_count));
    }
    seq_printf(m, "\n");

    seq_printf(m, "Prediction:\n");
    {
        struct predict_stats ps;
        predictive_get_stats(&ps);
        seq_printf(m, "  samples:        %llu\n", atomic64_read(&ps.sample_count));
        seq_printf(m, "  predictions:    %llu\n", atomic64_read(&ps.prediction_count));
        seq_printf(m, "  oom_predicts:   %llu\n", atomic64_read(&ps.oom_predict_count));
    }

    return 0;
}

static int stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, stats_show, NULL);
}

static const struct proc_ops stats_proc_ops = {
	.proc_open = stats_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/* hotspots 显示 */
static int hotspots_show(struct seq_file *m, void *v)
{
    struct hotspot_top_entry *top;
    int count, i;

    top = kmalloc_array(HOTSPOT_TOP_N, sizeof(struct hotspot_top_entry),
                        GFP_KERNEL);
    if (!top)
        return -ENOMEM;

    seq_printf(m, "SmartMemEngine Hotspots (Top %d)\n", HOTSPOT_TOP_N);
    seq_printf(m, "================================\n\n");

    count = smartmem_analysis_get_hotspots(top, HOTSPOT_TOP_N);
    if (count <= 0) {
        seq_printf(m, "No hotspot data available.\n");
        seq_printf(m, "Hotspots are tracked for slow allocations (>100us).\n");
        return 0;
    }

    for (i = 0; i < count; i++) {
        seq_printf(m, "--- Hotspot #%d (score=%llu) ---\n", i + 1, top[i].score);
        seq_printf(m, "  alloc_count:    %llu\n", top[i].alloc_count);
        seq_printf(m, "  alloc_pages:    %llu\n", top[i].alloc_pages);
        seq_printf(m, "  latency_avg:    %lluns\n", top[i].latency_avg_ns);
        seq_printf(m, "  latency_max:    %lluns\n", top[i].latency_max_ns);
        seq_printf(m, "  stack_hash:     0x%08x\n", top[i].stack_hash);
        seq_printf(m, "  call_stack:\n");

        /* 用 sprint_symbol 解析符号 */
        if (top[i].stack_depth > 0) {
            int j;
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

static int hotspots_open(struct inode *inode, struct file *file)
{
	return single_open(file, hotspots_show, NULL);
}

static const struct proc_ops hotspots_proc_ops = {
	.proc_open = hotspots_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/* 瓶颈分析显示 */
static int bottlenecks_show(struct seq_file *m, void *v)
{
    struct bottleneck_entry *entries;
    int count, i;

    entries = kmalloc_array(MAX_BOTTLENECKS, sizeof(struct bottleneck_entry),
                            GFP_KERNEL);
    if (!entries)
        return -ENOMEM;

    seq_printf(m, "SmartMemEngine Bottlenecks\n");
    seq_printf(m, "=========================\n\n");

    /* 读取前触发分析 */
    smartmem_analysis_bottleneck_update();

    count = smartmem_analysis_get_bottlenecks(entries, MAX_BOTTLENECKS);
    if (count <= 0) {
        seq_printf(m, "No bottlenecks detected.\n");
        kfree(entries);
        return 0;
    }

    for (i = 0; i < count; i++) {
        static const char *type_str[] = {
            "none", "slow_alloc", "high_fragment",
            "numa_imbalance", "low_slab_hit", "oom_risk"
        };
        static const char *sev_str[] = {
            "low", "medium", "high", "critical"
        };

        seq_printf(m, "--- Bottleneck #%d ---\n", i + 1);
        seq_printf(m, "  type:       %s\n",
                   entries[i].type < 6 ? type_str[entries[i].type] : "unknown");
        seq_printf(m, "  severity:   %s\n",
                   entries[i].severity < 4 ? sev_str[entries[i].severity] : "unknown");
        seq_printf(m, "  value:      %llu\n", entries[i].value);
        seq_printf(m, "  threshold:  %llu\n", entries[i].threshold);
        seq_printf(m, "  desc:       %s\n", entries[i].description);
        seq_printf(m, "\n");
    }

    kfree(entries);
    return 0;
}

static int bottlenecks_open(struct inode *inode, struct file *file)
{
    return single_open(file, bottlenecks_show, NULL);
}

static const struct proc_ops bottlenecks_proc_ops = {
    .proc_open = bottlenecks_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const char *rc_type_str[] = {
    "none", "kswapd_pressure", "direct_reclaim",
    "compact_fail", "slab_bloat", "misplaced_numa"
};

/* 根因分析显示 */
static int rootcauses_show(struct seq_file *m, void *v)
{
    struct root_cause_entry *entries;
    int count, i;

    entries = kmalloc_array(MAX_ROOT_CAUSES, sizeof(struct root_cause_entry),
                            GFP_KERNEL);
    if (!entries)
        return -ENOMEM;

    seq_printf(m, "SmartMemEngine Root Cause Analysis\n");
    seq_printf(m, "==================================\n\n");

    /* 读取前触发分析（瓶颈+根因） */
    smartmem_analysis_bottleneck_update();
    smartmem_analysis_root_cause_update();

    count = smartmem_analysis_get_root_causes(entries, MAX_ROOT_CAUSES);
    if (count <= 0) {
        seq_printf(m, "No root causes identified.\n");
        kfree(entries);
        return 0;
    }

    for (i = 0; i < count; i++) {
        seq_printf(m, "--- Root Cause #%d ---\n", i + 1);
        seq_printf(m, "  type:        %s\n", entries[i].type < 6 ? rc_type_str[entries[i].type] : "unknown");
        seq_printf(m, "  confidence:  %llu%%\n", entries[i].confidence);
        seq_printf(m, "  desc:        %s\n", entries[i].description);
        seq_printf(m, "  suggestion:  %s\n", entries[i].suggestion);
        seq_printf(m, "\n");
    }

    kfree(entries);
    return 0;
}

static int rootcauses_open(struct inode *inode, struct file *file)
{
    return single_open(file, rootcauses_show, NULL);
}

static const struct proc_ops rootcauses_proc_ops = {
    .proc_open = rootcauses_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const char *tune_action_str[] = {
    "none", "compact", "adjust_watermark",
    "numa_migrate", "adjust_slab"
};

/* 自动调优状态显示 */
static int autotune_show(struct seq_file *m, void *v)
{
    struct tune_stats stats;
    struct tune_history *history;
    int count, i;

    seq_printf(m, "SmartMemEngine Auto-Tune\n");
    seq_printf(m, "========================\n\n");

    /* 显示统计 */
    auto_tune_get_stats(&stats);
    seq_printf(m, "Statistics:\n");
    seq_printf(m, "  total_tunes:    %llu\n", atomic64_read(&stats.total_tune_count));
    seq_printf(m, "  compact:        %llu\n", atomic64_read(&stats.compact_count));
    seq_printf(m, "  watermark:      %llu\n", atomic64_read(&stats.watermark_count));
    seq_printf(m, "  numa_migrate:   %llu\n", atomic64_read(&stats.numa_migrate_count));
    seq_printf(m, "  slab_adjust:    %llu\n", atomic64_read(&stats.slab_adjust_count));
    seq_printf(m, "  failures:       %llu\n", atomic64_read(&stats.fail_count));
    seq_printf(m, "\n");

    /* 显示历史 */
    history = kmalloc_array(MAX_TUNE_HISTORY, sizeof(struct tune_history),
                            GFP_KERNEL);
    if (!history)
        return -ENOMEM;

    count = auto_tune_get_history(history, MAX_TUNE_HISTORY);
    if (count > 0) {
        seq_printf(m, "Recent Tune History:\n");
        for (i = 0; i < count; i++) {
            seq_printf(m, "  [%d] action=%-16s result=%d  %s\n",
                       i + 1,
                       history[i].action < 5 ?
                           tune_action_str[history[i].action] : "unknown",
                       history[i].result,
                       history[i].description);
        }
    } else {
        seq_printf(m, "No tune history yet.\n");
    }

    kfree(history);
    return 0;
}

static int autotune_open(struct inode *inode, struct file *file)
{
    return single_open(file, autotune_show, NULL);
}

/* autotune 写入：手动触发调优 */
static ssize_t autotune_write(struct file *file, const char __user *buffer,
                               size_t count, loff_t *ppos)
{
    char buf[32];
    int action;

    if (count > sizeof(buf) - 1)
        return -EINVAL;

    if (copy_from_user(buf, buffer, count))
        return -EFAULT;

    buf[count] = '\0';
    if (buf[count - 1] == '\n')
        buf[count - 1] = '\0';

    if (strcmp(buf, "compact") == 0)
        action = TUNE_ACTION_COMPACT;
    else if (strcmp(buf, "watermark") == 0)
        action = TUNE_ACTION_ADJUST_WATERMARK;
    else if (strcmp(buf, "numa") == 0)
        action = TUNE_ACTION_NUMA_MIGRATE;
    else if (strcmp(buf, "slab") == 0)
        action = TUNE_ACTION_ADJUST_SLAB;
    else if (strcmp(buf, "check") == 0)
        return count;  /* check 通过读取触发 */
    else
        return -EINVAL;

    auto_tune_trigger(action);

    return count;
}

static const struct proc_ops autotune_proc_ops = {
    .proc_open = autotune_open,
    .proc_read = seq_read,
    .proc_write = autotune_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const char *predict_type_str[] = {
    "none", "oom_risk", "memory_exhaust", "pressure_increase"
};
static const char *predict_sev_str[] = {
    "low", "medium", "high"
};

/* 预测结果显示 */
static int prediction_show(struct seq_file *m, void *v)
{
    struct prediction_result result;
    struct predict_stats stats;
    int ret;

    seq_printf(m, "SmartMemEngine Memory Prediction\n");
    seq_printf(m, "================================\n\n");

    /* 执行预测 */
    ret = predictive_predict(&result);
    if (ret) {
        seq_printf(m, "Prediction failed: %d\n", ret);
        return 0;
    }

    seq_printf(m, "Prediction Result:\n");
    seq_printf(m, "  type:            %s\n",
               result.type < 4 ? predict_type_str[result.type] : "unknown");
    seq_printf(m, "  severity:        %s\n",
               result.severity < 3 ? predict_sev_str[result.severity] : "unknown");
    {
        unsigned long total = totalram_pages();
        int cur_free = total > 0 ?
            (int)((global_zone_page_state(NR_FREE_PAGES) * 100) / total) : 0;
        seq_printf(m, "  current_free:    %d%%\n", cur_free);
    }
    seq_printf(m, "  predicted_free:  %llu%%\n", result.predicted_free_pct);
    seq_printf(m, "  estimated_time:  %llus\n", result.estimated_time_sec);
    seq_printf(m, "  confidence:      %llu%%\n", result.confidence);
    seq_printf(m, "  description:     %s\n", result.description);
    seq_printf(m, "\n");

    /* 显示采样统计 */
    predictive_get_stats(&stats);
    seq_printf(m, "Model Statistics:\n");
    seq_printf(m, "  samples:         %llu\n", atomic64_read(&stats.sample_count));
    seq_printf(m, "  predictions:     %llu\n", atomic64_read(&stats.prediction_count));
    seq_printf(m, "  oom_predictions: %llu\n", atomic64_read(&stats.oom_predict_count));

    return 0;
}

static int prediction_open(struct inode *inode, struct file *file)
{
    return single_open(file, prediction_show, NULL);
}

static const struct proc_ops prediction_proc_ops = {
    .proc_open = prediction_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/* control 写入处理 */
static ssize_t control_write(struct file *file, const char __user *buffer,
                              size_t count, loff_t *ppos)
{
    char buf[128];
    char *cmd, *arg;

    if (count > sizeof(buf) - 1)
        return -EINVAL;

    if (copy_from_user(buf, buffer, count))
        return -EFAULT;

    buf[count] = '\0';
    if (count > 0 && buf[count - 1] == '\n')
        buf[count - 1] = '\0';

    /* 解析 "cmd arg" 格式 */
    cmd = buf;
    arg = strchr(buf, ' ');
    if (arg) {
        *arg++ = '\0';
    }

    if (strcmp(cmd, "enable") == 0 && arg) {
        /* 启用功能: enable hook_buddy, enable trace, enable autotune */
        smartmem_config_set(arg, "true");
        pr_info("smartmem: control: enabled %s\n", arg);

        /* 对 autotune 特殊处理 */
        if (strcmp(arg, "auto_tune_enabled") == 0)
            auto_tune_start();
        if (strcmp(arg, "trace_enabled") == 0)
            smartmem_monitor_start();

    } else if (strcmp(cmd, "disable") == 0 && arg) {
        /* 禁用功能: disable hook_buddy, disable trace, disable autotune */
        smartmem_config_set(arg, "false");
        pr_info("smartmem: control: disabled %s\n", arg);

        if (strcmp(arg, "auto_tune_enabled") == 0)
            auto_tune_stop();
        if (strcmp(arg, "trace_enabled") == 0)
            smartmem_monitor_stop();

    } else if (strcmp(cmd, "reset") == 0 && arg) {
        /* 重置: reset stats, reset hotspots, reset autotune */
        if (strcmp(arg, "stats") == 0) {
            smartmem_stats_reset();
            trace_monitor_reset_stats();
            auto_tune_reset_stats();
            predictive_reset();
            pr_info("smartmem: control: all stats reset\n");
        } else if (strcmp(arg, "hotspots") == 0) {
            hotspot_reset();
            pr_info("smartmem: control: hotspots reset\n");
        } else if (strcmp(arg, "autotune") == 0) {
            auto_tune_reset_stats();
            pr_info("smartmem: control: autotune stats reset\n");
        } else if (strcmp(arg, "prediction") == 0) {
            predictive_reset();
            pr_info("smartmem: control: prediction model reset\n");
        } else {
            return -EINVAL;
        }

    } else if (strcmp(cmd, "tune") == 0 && arg) {
        /* 手动调优: tune compact, tune watermark, tune numa, tune slab */
        if (strcmp(arg, "compact") == 0)
            auto_tune_trigger(TUNE_ACTION_COMPACT);
        else if (strcmp(arg, "watermark") == 0)
            auto_tune_trigger(TUNE_ACTION_ADJUST_WATERMARK);
        else if (strcmp(arg, "numa") == 0)
            auto_tune_trigger(TUNE_ACTION_NUMA_MIGRATE);
        else if (strcmp(arg, "slab") == 0)
            auto_tune_trigger(TUNE_ACTION_ADJUST_SLAB);
        else
            return -EINVAL;

    } else if (strcmp(cmd, "set") == 0 && arg) {
        /* 设置配置: set key=value (透传给 config) */
        char *val = strchr(arg, '=');
        if (!val)
            return -EINVAL;
        *val++ = '\0';
        smartmem_config_set(arg, val);
        pr_info("smartmem: control: set %s=%s\n", arg, val);

    } else {
        return -EINVAL;
    }

    return count;
}

static int control_show(struct seq_file *m, void *v)
{
    seq_printf(m, "SmartMemEngine Control\n");
    seq_printf(m, "======================\n\n");
    seq_printf(m, "Commands (write to this file):\n");
    seq_printf(m, "  enable <feature>    - Enable a feature\n");
    seq_printf(m, "  disable <feature>   - Disable a feature\n");
    seq_printf(m, "  reset <target>      - Reset stats/hotspots/autotune/prediction\n");
    seq_printf(m, "  tune <action>       - Trigger manual tune (compact/watermark/numa/slab)\n");
    seq_printf(m, "  set <key>=<value>   - Set configuration\n");
    seq_printf(m, "\n");
    seq_printf(m, "Features: hook_buddy_enabled, hook_slub_enabled, hook_vma_enabled,\n");
    seq_printf(m, "          hook_lru_enabled, hook_numa_enabled, numa_aware_enabled,\n");
    seq_printf(m, "          adaptive_slub_enabled, ebpf_enabled, trace_enabled,\n");
    seq_printf(m, "          auto_tune_enabled\n");
    seq_printf(m, "\n");
    seq_printf(m, "Examples:\n");
    seq_printf(m, "  echo 'enable auto_tune_enabled' > /proc/smartmem/control\n");
    seq_printf(m, "  echo 'disable trace_enabled' > /proc/smartmem/control\n");
    seq_printf(m, "  echo 'reset stats' > /proc/smartmem/control\n");
    seq_printf(m, "  echo 'tune compact' > /proc/smartmem/control\n");

    return 0;
}

static int control_open(struct inode *inode, struct file *file)
{
    return single_open(file, control_show, NULL);
}

static const struct proc_ops control_proc_ops = {
    .proc_open = control_open,
    .proc_read = seq_read,
    .proc_write = control_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/**
 * procfs初始化
 */
int smartmem_proc_init(void)
{
    pr_info("smartmem: procfs initializing...\n");

    /* 创建 /proc/smartmem 目录 */
    smartmem_dir = proc_mkdir("smartmem", NULL);
    if (!smartmem_dir) {
        pr_err("smartmem: failed to create /proc/smartmem\n");
        return -ENOMEM;
    }

    /* 创建 /proc/smartmem/config */
    if (!proc_create("config", 0644, smartmem_dir, &config_proc_ops)) {
        pr_err("smartmem: failed to create /proc/smartmem/config\n");
        remove_proc_entry("smartmem", NULL);
        return -ENOMEM;
    }

    if (!proc_create("control", 0644, smartmem_dir, &control_proc_ops)) {
        pr_err("smartmem: failed to create /proc/smartmem/control\n");
        remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("smartmem", NULL);
        return -ENOMEM;
    }

    // 创建 /proc/smartmem/policies
    if (!proc_create("policies", 0444, smartmem_dir, &policies_proc_ops)) {
        pr_err("smartmem: failed to create /prco/smartmem/policies\n");
        remove_proc_entry("control", smartmem_dir);
        remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("smartmem", NULL);
        return -ENOMEM;
    }

    if (!proc_create("stats", 0444, smartmem_dir, &stats_proc_ops)) {
		pr_err("smartmem: failed to create /proc/smartmem/stats\n");
        remove_proc_entry("policies", smartmem_dir);
        remove_proc_entry("control", smartmem_dir);
		remove_proc_entry("config", smartmem_dir);
		remove_proc_entry("smartmem", NULL);
		return -ENOMEM;
	}

    if (!proc_create("hotspots", 0444, smartmem_dir, &hotspots_proc_ops)) {
		pr_err("smartmem: failed to create /proc/smartmem/hotspots\n");
		remove_proc_entry("stats", smartmem_dir);
        remove_proc_entry("policies", smartmem_dir);
        remove_proc_entry("control", smartmem_dir);
		remove_proc_entry("config", smartmem_dir);
		remove_proc_entry("smartmem", NULL);
		return -ENOMEM;
	}

    if (!proc_create("bottlenecks", 0444, smartmem_dir, &bottlenecks_proc_ops)) {
        pr_err("smartmem: failed to create /proc/smartmem/bottlenecks\n");
        remove_proc_entry("hotspots", smartmem_dir);
        remove_proc_entry("stats", smartmem_dir);
        remove_proc_entry("policies", smartmem_dir);
        remove_proc_entry("control", smartmem_dir);
        remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("smartmem", NULL);
        return -ENOMEM;
    }

    if (!proc_create("rootcauses", 0444, smartmem_dir, &rootcauses_proc_ops)) {
        pr_err("smartmem: failed to create /proc/smartmem/rootcauses\n");
        remove_proc_entry("bottlenecks", smartmem_dir);
        remove_proc_entry("hotspots", smartmem_dir);
        remove_proc_entry("stats", smartmem_dir);
        remove_proc_entry("policies", smartmem_dir);
        remove_proc_entry("control", smartmem_dir);
        remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("smartmem", NULL);
        return -ENOMEM;
    }

    if (!proc_create("autotune", 0644, smartmem_dir, &autotune_proc_ops)) {
        pr_err("smartmem: failed to create /proc/smartmem/autotune\n");
        remove_proc_entry("rootcauses", smartmem_dir);
        remove_proc_entry("bottlenecks", smartmem_dir);
        remove_proc_entry("hotspots", smartmem_dir);
        remove_proc_entry("stats", smartmem_dir);
        remove_proc_entry("policies", smartmem_dir);
        remove_proc_entry("control", smartmem_dir);
        remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("smartmem", NULL);
        return -ENOMEM;
    }

    if (!proc_create("prediction", 0444, smartmem_dir, &prediction_proc_ops)) {
        pr_err("smartmem: failed to create /proc/smartmem/prediction\n");
        remove_proc_entry("autotune", smartmem_dir);
        remove_proc_entry("rootcauses", smartmem_dir);
        remove_proc_entry("bottlenecks", smartmem_dir);
        remove_proc_entry("hotspots", smartmem_dir);
        remove_proc_entry("stats", smartmem_dir);
        remove_proc_entry("policies", smartmem_dir);
        remove_proc_entry("control", smartmem_dir);
        remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("smartmem", NULL);
        return -ENOMEM;
    }

    pr_info("smartmem: procfs initialized\n");
    return 0;
}

/* procfs 退出 */
void smartmem_proc_exit(void)
{
    pr_info("smartmem: procfs exiting...\n");

    if (smartmem_dir) {
        remove_proc_entry("prediction", smartmem_dir);
        remove_proc_entry("autotune", smartmem_dir);
        remove_proc_entry("rootcauses", smartmem_dir);
        remove_proc_entry("bottlenecks", smartmem_dir);
        remove_proc_entry("hotspots", smartmem_dir);
        remove_proc_entry("stats", smartmem_dir);
        remove_proc_entry("policies", smartmem_dir);
        remove_proc_entry("control", smartmem_dir);
        remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("smartmem", NULL);
        smartmem_dir = NULL;
    }

    pr_info("smartmem: procfs exited\n");
}