/* eBPF监控实现 */

#include "ebpf_monitor.h"

static bool ebpf_initialized = false;
// static bool ebpf_loaded = false;
// static bool ebpf_running = false;

/**
 * eBPF监控初始化
 */
int ebpf_monitor_init(void)
{
    pr_info("smartmem: eBPF monitor initializing...\n");

    /*TODO:  调用BPF加载器加载BPF程序 */

    ebpf_initialized = true;

    pr_info("smartmem: eBPF monitor initialized\n");
    return 0;
}

/**
 * eBPF监控退出
 */
void ebpf_monitor_exit(void)
{
    pr_info("smartmem: eBPF monitor exiting...\n");

    ebpf_initialized = false;

    pr_info("smartmem: eBPF monitor exited\n");
}

/**
 * 启动eBPF监控
 */
int ebpf_monitor_start(void)
{
    pr_info("smartmem: eBPF monitor started\n");
    return 0;
}

/**
 * 停止eBPF监控
 */
int ebpf_monitor_stop(void)
{
    pr_info("smartmem: eBPF monitor stopped\n");
    return 0;
}