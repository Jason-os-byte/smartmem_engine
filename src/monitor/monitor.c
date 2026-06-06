/* 监控系统实现
 *
 * 由两个独立子系统组成：
 *   1) trace_monitor (内核内置 tracepoint hooks)
 *   2) ebpf_status   (用户态 libbpf loader 的状态通道)
 *
 * BPF 程序本身在用户态加载（tools/memtrace），内核模块只接收心跳。
 */

#include "monitor.h"
#include "ebpf_status.h"
#include "trace_monitor.h"

static struct smartmem_monitor g_monitor = {
    .ebpf_enabled = false,
    .trace_enabled = false,
    .initialized = false,
};

/**
 * 监控系统初始化
 */
int smartmem_monitor_init(void)
{
    int ret;

    pr_info("smartmem: monitor initializing...\n");

    /* eBPF 状态通道（始终可用，等待 loader 接入） */
    ret = ebpf_status_init();
    if (ret) {
        pr_warn("smartmem: ebpf status init failed\n");
    } else {
        g_monitor.ebpf_enabled = true;
    }

    /* 初始化 tracepoint 监控 */
    ret = trace_monitor_init();
    if (ret) {
        pr_warn("smartmem: trace monitor init failed, continuing without it\n");
    } else {
        g_monitor.trace_enabled = true;
    }

    g_monitor.initialized = true;

    pr_info("smartmem: monitor initialized (ebpf_channel=%s, trace=%s)\n",
            g_monitor.ebpf_enabled ? "yes" : "no",
            g_monitor.trace_enabled ? "yes" : "no");
    return 0;
}

/**
 * 监控系统退出
 */
void smartmem_monitor_exit(void)
{
    pr_info("smartmem: monitor exiting...\n");

    if (g_monitor.trace_enabled) {
        trace_monitor_exit();
        g_monitor.trace_enabled = false;
    }

    if (g_monitor.ebpf_enabled) {
        ebpf_status_exit();
        g_monitor.ebpf_enabled = false;
    }

    g_monitor.initialized = false;

    pr_info("smartmem: monitor exited\n");
}

/**
 * 启动监控（只影响 trace_monitor；BPF 由用户态控制）
 */
int smartmem_monitor_start(void)
{
    int ret;

    pr_info("smartmem: starting monitor...\n");

    if (g_monitor.trace_enabled) {
        ret = trace_monitor_start();
        if (ret) {
            pr_warn("smartmem: failed to start trace monitor\n");
        }
    }

    pr_info("smartmem: monitor started\n");
    return 0;
}

/**
 * 停止监控
 */
int smartmem_monitor_stop(void)
{
    pr_info("smartmem: stopping monitor...\n");

    if (g_monitor.trace_enabled) {
        trace_monitor_stop();
    }

    pr_info("smartmem: monitor stopped\n");
    return 0;
}
