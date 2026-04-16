/* procfs接口实现 */

#include "proc.h"
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static struct proc_dir_entry *smartmem_dir = NULL;

/**
 * 显示配置
 */
static int config_show(struct seq_file *m, void *v)
{
    seq_printf(m, "SmartMemEngine Configuration\n");
    seq_printf(m, "============================\n");
    seq_printf(m, "Version: 1.0.0\n");
    seq_printf(m, "Status: Running\n");
    return 0;
}

/**
 * 打开配置文件
 */
static int config_open(struct inode *inode, struct file *file)
{
    return single_open(file, config_show, NULL);
}

static const struct proc_ops config_proc_ops = {
    .proc_open = config_open,
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

    pr_info("smartmem: procfs initialized\n");
    return 0;
}

/* procfs 退出 */
void smartmem_proc_exit(void)
{
    pr_info("smartmem: procfs exiting...\n");

    if (smartmem_dir) {
        remove_proc_entry("config", smartmem_dir);
        remove_proc_entry("smartmem", NULL);
        smartmem_dir = NULL;
    }

    pr_info("smartmem: procfs exited\n");
}