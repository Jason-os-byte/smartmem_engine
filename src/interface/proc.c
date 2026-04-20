/* procfs接口实现 */

#include "proc.h"
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "config.h"
#include "strategy.h"
#include "stats.h"

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
    struct buddy_strategy *s;

    seq_printf(m, "SmartmemEngine Policies\n");
    seq_printf(m, "=======================\n");

    seq_printf(m, "Buddy Strategies:\n");
    s = buddy_strategy_get_current();
    if (s) {
        seq_printf(m, "  Current: %s (enabled)\n", s->name);
        seq_printf(m, "  Version: %s\n", s->version);
        seq_printf(m, "  Description: %s\n", s->description);
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

    // 创建 /proc/smartmem/policies
    if (!proc_create("policies", 0444, smartmem_dir, &policies_proc_ops)) {
        pr_err("smartmem: failed to create /prco/smartmem/policies\n");
        remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("smartmem", NULL);
        return -ENOMEM;
    }

    if (!proc_create("stats", 0444, smartmem_dir, &stats_proc_ops)) {
		pr_err("smartmem: failed to create /proc/smartmem/stats\n");
		remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("policies", smartmem_dir);
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
        remove_proc_entry("stats", smartmem_dir);
        remove_proc_entry("policies", smartmem_dir);
        remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("smartmem", NULL);
        smartmem_dir = NULL;
    }

    pr_info("smartmem: procfs exited\n");
}