/* trace_monitor.c - tracepoint 监控实现
 * 独立统计，不与 Hook 层重复计数
 */

#include "trace_monitor.h"
#include <linux/tracepoint.h>
#include <linux/string.h>
#include <linux/trace_events.h>

static atomic64_t trace_event_count;
static atomic64_t trace_alloc_count;
static atomic64_t trace_free_count;
static bool trace_initialized = false;
static bool trace_running = false;

/**
 * mm_page_alloc 回调
 */
static void trace_tp_page_alloc(void *__data, struct page *page, unsigned int order)
{
	atomic64_inc(&trace_event_count);
	atomic64_inc(&trace_alloc_count);
}

/**
 * mm_page_free 回调
 */
static void trace_tp_page_free(void *__data, struct page *page, unsigned int order)
{
	atomic64_inc(&trace_event_count);
	atomic64_inc(&trace_free_count);
}

/* tracepoint 探针 */
struct trace_tp_probe {
	const char *name;
	struct tracepoint *tp;
	void *probe;
	bool registered;
};

static struct trace_tp_probe trace_probes[] = {
	{
		.name = "mm_page_alloc",
		.probe = trace_tp_page_alloc,
		.registered = false,
	},
	{
		.name = "mm_page_free",
		.probe = trace_tp_page_free,
		.registered = false,
	},
};

#define NUM_TRACE_PROBES (sizeof(trace_probes) / sizeof(trace_probes[0]))

/* tracepoint 查找 */
static struct tracepoint *found_tp = NULL;
static const char *search_name = NULL;

static void find_tp_callback(struct tracepoint *tp, void *priv)
{
	if (strcmp(tp->name, search_name) == 0)
		found_tp = tp;
}

static int find_tracepoint(struct trace_tp_probe *p)
{
	found_tp = NULL;
	search_name = p->name;

	for_each_kernel_tracepoint(find_tp_callback, NULL);

	if (!found_tp) {
		pr_warn("smartmem-trace: tracepoint '%s' not found\n", p->name);
		return -ENOENT;
	}

	p->tp = found_tp;
	return 0;
}

/**
 * tracepoint 监控初始化
 */
int trace_monitor_init(void)
{
	int i;
	int ret;
	int found = 0;

	pr_info("smartmem: trace monitor initializing...\n");

	atomic64_set(&trace_event_count, 0);
	atomic64_set(&trace_alloc_count, 0);
	atomic64_set(&trace_free_count, 0);

	for (i = 0; i < NUM_TRACE_PROBES; i++) {
		ret = find_tracepoint(&trace_probes[i]);
		if (ret == 0)
			found++;
	}

	if (found == 0) {
		pr_warn("smartmem: no tracepoints found, trace monitor disabled\n");
		return -ENOENT;
	}

	trace_initialized = true;

	pr_info("smartmem: trace monitor initialized (%d/%zu tracepoints found)\n",
		found, NUM_TRACE_PROBES);
	return 0;
}

/**
 * tracepoint 监控退出
 */
void trace_monitor_exit(void)
{
	pr_info("smartmem: trace monitor exiting...\n");

	if (trace_running)
		trace_monitor_stop();

	trace_initialized = false;

	pr_info("smartmem: trace monitor exited (events=%llu, allocs=%llu, frees=%llu)\n",
		atomic64_read(&trace_event_count),
		atomic64_read(&trace_alloc_count),
		atomic64_read(&trace_free_count));
}

/**
 * 启动 tracepoint 监控
 */
int trace_monitor_start(void)
{
	int i;
	int ret;
	int registered = 0;

	pr_info("smartmem: trace monitor starting...\n");

	if (!trace_initialized) {
		pr_err("smartmem: trace monitor not initialized\n");
		return -EINVAL;
	}

	for (i = 0; i < NUM_TRACE_PROBES; i++) {
		if (!trace_probes[i].tp)
			continue;

		if (trace_probes[i].registered) {
			registered++;
			continue;
		}

		ret = tracepoint_probe_register(trace_probes[i].tp, trace_probes[i].probe, NULL);
		if (ret == -EEXIST) {
			/* 探针已存在（可能是上次卸载后 RCU 宽限期未过） */
			trace_probes[i].registered = true;
			registered++;
			pr_info("smartmem: trace probe '%s' already registered\n",
				trace_probes[i].name);
		} else if (ret) {
			pr_warn("smartmem: failed to register trace probe '%s': %d\n",
				trace_probes[i].name, ret);
		} else {
			trace_probes[i].registered = true;
			registered++;
		}
	}

	if (registered == 0) {
		pr_err("smartmem: no trace probes registered\n");
		return -ENOENT;
	}

	trace_running = true;

	pr_info("smartmem: trace monitor started (%d probes registered)\n", registered);
	return 0;
}

/**
 * 停止 tracepoint 监控
 */
int trace_monitor_stop(void)
{
	int i;

	pr_info("smartmem: trace monitor stopping...\n");

	for (i = 0; i < NUM_TRACE_PROBES; i++) {
		if (trace_probes[i].registered && trace_probes[i].tp) {
			tracepoint_probe_unregister(trace_probes[i].tp, trace_probes[i].probe, NULL);
			trace_probes[i].registered = false;
		}
	}

	/* 等待 RCU 宽限期结束，确保探针完全移除 */
	tracepoint_synchronize_unregister();

	trace_running = false;

	pr_info("smartmem: trace monitor stopped\n");
	return 0;
}

/**
 * 读取 tracepoint 统计
 */
u64 trace_monitor_get_event_count(void)
{
	return atomic64_read(&trace_event_count);
}

u64 trace_monitor_get_alloc_count(void)
{
	return atomic64_read(&trace_alloc_count);
}

u64 trace_monitor_get_free_count(void)
{
	return atomic64_read(&trace_free_count);
}

/**
 * 重置 tracepoint 统计
 */
void trace_monitor_reset_stats(void)
{
	atomic64_set(&trace_event_count, 0);
	atomic64_set(&trace_alloc_count, 0);
	atomic64_set(&trace_free_count, 0);

	pr_info("smartmem: trace monitor stats reset\n");
}
