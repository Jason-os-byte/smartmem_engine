/* debugfs接口实现 */

#include "debugfs.h"
#include <linux/debugfs.h>
#include <linux/seq_file.h>

static struct dentry *smartmem_debug_dir = NULL;

/**
 * 显示状态
 */
static int status_show(struct seq_file *m, void *v)
{
    seq_printf(m, "SmartMemEngine Debug Status\n");
    seq_printf(m, "===========================\n");
    seq_printf(m, "State: Ready\n");
    seq_printf(m, "Hooks: Enabled\n");
    seq_printf(m, "Strategies: Loaded\n");
    return 0;
}

/**
 * 打开状态文件
 */
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
 * debugfs初始化
 */
int smartmem_debugfs_init(void)
{
    pr_info("smartmem: debugfs initializing...\n");

    /* 创建 /sys/kernel/debug/smartmem 目录 */
    smartmem_debug_dir = debugfs_create_dir("smartmem", NULL);
    if (!smartmem_debug_dir) {
        pr_err("smartmem: failed to create /sys/kernel/debug/smartmem\n");
        return -ENOMEM;
    }

    /* 创建 /sys/kernel/debug/smartmem/status */
    if (!debugfs_create_file("status", 0444, smartmem_debug_dir, NULL, &status_fops)) {
        pr_err("smartmem: failed to create /sys/kernel/debug/status\n");
        debugfs_remove_recursive(smartmem_debug_dir);
        return -ENOMEM;
    }

    pr_info("smartmem: debugfs initialized (/sys/kernel/debug/smartmem/)\n");
    return 0;
}

/**
 * debugfs退出
 */
void smartmem_debugfs_exit(void)
{
    pr_info("smartmem: debugfs exiting...\n");

    if (smartmem_debug_dir) {
        debugfs_remove_recursive(smartmem_debug_dir);
        smartmem_debug_dir = NULL;
    }

    pr_info("smartmem: debugfs exited\n");
}
