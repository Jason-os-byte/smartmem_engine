/* 接口层实现 */

#include "interface.h"
#include "proc.h"
#include "debugfs.h"

static struct smartmem_interface g_interface = {
    .proc_enabled = false,
    .debugfs_enabled = false,
    .netlink_enabled = false,
    .initialized = false,
};

/**
 * 接口层初始化
 */
int smartmem_interface_init(void)
{
    int ret;

    pr_info("smartmem: interface layer initializing...\n");

    /* 初始化 procfs */
    ret = smartmem_proc_init();
    if (ret) {
        pr_warn("smartmem: procfs init failed\n");
    } else {
        g_interface.proc_enabled = true;
    }

    /* 初始化 debugfs */
    ret = smartmem_debugfs_init();
    if (ret) {
        pr_warn("smartmem: debugfs init failed\n");
    } else {
        g_interface.debugfs_enabled = true;
    }

    g_interface.initialized = true;

    pr_info("smartmem: interface layer initialized (proc=%s, debugfs=%s)\n",
            g_interface.proc_enabled ? "yes" : "no",
            g_interface.debugfs_enabled ? "yes" : "no");
    return 0;
}

/**
 * 接口层退出
 */
void smartmem_interface_exit(void)
{
    pr_info("smartmem: interface layer exiting...\n");

    if (g_interface.debugfs_enabled) {
        smartmem_debugfs_exit();
        g_interface.debugfs_enabled = false;
    }

    if (g_interface.proc_enabled) {
        smartmem_proc_exit();
        g_interface.proc_enabled = false;
    }

    g_interface.initialized = false;

    pr_info("smartmem: interface layer exited\n");
}

