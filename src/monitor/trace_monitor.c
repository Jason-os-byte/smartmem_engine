/* tracepoint监控实现 */

#include "trace_monitor.h"

static bool trace_initialized = false;
static bool trace_running = false;

/**
 * tracepoint监控初始化
 */
int trace_monitor_init(void)
{
    pr_info("smartmem: trace monitor initializing...\n");

    /* TODO: 初始化tracepoint */

    trace_initialized = true;

    pr_info("smartmem: trace monitor initialized\n");
    return 0;
}

/**
 * tracepoint监控退出
 */
void trace_monitor_exit(void)
{
    pr_info("smartmem: trace monitor exiting...\n");

    if (trace_running) {
        trace_monitor_stop();
    }

    trace_initialized = false;

    pr_info("smartmem: trace monitor exited\n");
}

/**
 * 启动tracepoint监控
 */
int trace_monitor_start(void)
{
    pr_info("smartmem: starting trace monitor...\n");

    /* TODO: 启用tracepoint */

    trace_running = true;

    pr_info("smartmem: trace monitor started\n");
    return 0;
}

/**
 * 停止tracepoint
 */
int trace_monitor_stop(void)
{
    pr_info("smartmem: stopping trace monitor...\n");

    /* TODO: 禁用tracepoint */

    trace_running = false;

    pr_info("smartmem: trace monitor stopped\n");
    return 0;
}

