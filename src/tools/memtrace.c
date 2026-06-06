/* memtrace.c — SmartMemEngine eBPF 用户态加载器
 *
 * 功能:
 *   1. 通过 libbpf 加载并 attach src/bpf/smartmem.bpf.c 中的程序
 *   2. epoll 消费 BPF ringbuf，打印事件 (alloc/free + 延迟)
 *   3. 周期性 (1Hz) 把累计统计写入 /proc/smartmem/ebpf_status
 *
 * 编译: gcc -O2 -Wall -o memtrace memtrace.c -lbpf -lelf -lz
 *
 * 运行需要:
 *   - smartmem 内核模块已加载 (/proc/smartmem/ebpf_status 存在)
 *   - root 权限 (CAP_BPF + CAP_PERFMON)
 *   - 内核支持 CO-RE BTF (CONFIG_DEBUG_INFO_BTF)
 */

#include <argp.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "smartmem.skel.h"

/* 必须与 smartmem.bpf.c 中的 struct sm_event 二进制兼容 */
enum sm_event_type {
	SM_EVENT_BUDDY_ALLOC = 1,
	SM_EVENT_BUDDY_FREE,
	SM_EVENT_PAGE_FAULT,
};

struct sm_event {
	__u32 type;
	__u32 pid;
	__u64 timestamp;
	__u64 stack_id;
	union {
		struct {
			__u32 order;
			__u32 gfp_mask;
			__u64 latency_ns;
		} buddy_alloc;
		struct {
			__u32 order;
		} buddy_free;
	};
} __attribute__((packed));

/* 与 bpf 端 enum stat_key 对齐 */
enum stat_key {
	STAT_ALLOC_CNT     = 0,
	STAT_FREE_CNT      = 1,
	STAT_EVENT_DROPPED = 2,
};

#define EBPF_STATUS_PATH "/proc/smartmem/ebpf_status"

static volatile sig_atomic_t g_exit;

/* 累计计数（userland 侧，从 ringbuf 消费得到） */
static __u64 g_events_total;
static __u64 g_alloc_count;
static __u64 g_free_count;

/* 程序行为开关 */
static int g_verbose;        /* 1=逐事件打印 */
static int g_quiet;          /* 1=仅心跳，不打印事件 */

/* ---------------- libbpf 日志回调 ---------------- */
static int libbpf_print_fn(enum libbpf_print_level level,
			   const char *fmt, va_list args)
{
	if (level == LIBBPF_DEBUG && !g_verbose)
		return 0;
	return vfprintf(stderr, fmt, args);
}

/* ---------------- ringbuf 事件回调 ---------------- */
static int handle_event(void *ctx, void *data, size_t data_sz)
{
	const struct sm_event *e = data;

	(void)ctx;
	if (data_sz < sizeof(*e))
		return 0;

	g_events_total++;

	switch (e->type) {
	case SM_EVENT_BUDDY_ALLOC:
		g_alloc_count++;
		if (!g_quiet) {
			printf("[ALLOC ] pid=%-6u order=%-2u "
			       "gfp=0x%08x lat=%llu ns stack_id=%llu\n",
			       e->pid,
			       e->buddy_alloc.order,
			       e->buddy_alloc.gfp_mask,
			       (unsigned long long)e->buddy_alloc.latency_ns,
			       (unsigned long long)e->stack_id);
		}
		break;
	case SM_EVENT_BUDDY_FREE:
		g_free_count++;
		if (!g_quiet && g_verbose) {
			printf("[FREE  ] pid=%-6u order=%u\n",
			       e->pid, e->buddy_free.order);
		}
		break;
	default:
		break;
	}
	return 0;
}

/* ---------------- 心跳上报到 /proc/smartmem/ebpf_status ---------------- */
static __u64 read_dropped(struct smartmem_bpf *skel)
{
	int fd = bpf_map__fd(skel->maps.stats);
	__u32 key = STAT_EVENT_DROPPED;
	int n = libbpf_num_possible_cpus();
	__u64 sum = 0;
	int i;

	if (n < 1 || fd < 0)
		return 0;
	{
		__u64 vals[n];
		memset(vals, 0, sizeof(vals));
		if (bpf_map_lookup_elem(fd, &key, vals) == 0)
			for (i = 0; i < n; i++)
				sum += vals[i];
	}
	return sum;
}

static void push_heartbeat(struct smartmem_bpf *skel)
{
	FILE *fp;
	__u64 drops = read_dropped(skel);

	fp = fopen(EBPF_STATUS_PATH, "w");
	if (!fp) {
		/* 内核模块未加载或权限不够 — 静默退化 */
		return;
	}
	/* 格式: <pid> <events> <drops> <allocs> <frees> */
	fprintf(fp, "%d %llu %llu %llu %llu\n",
		(int)getpid(),
		(unsigned long long)g_events_total,
		(unsigned long long)drops,
		(unsigned long long)g_alloc_count,
		(unsigned long long)g_free_count);
	fclose(fp);
}

/* ---------------- 信号处理 ---------------- */
static void sigh(int sig)
{
	(void)sig;
	g_exit = 1;
}

/* ---------------- 提升 RLIMIT_MEMLOCK ---------------- */
static int bump_memlock(void)
{
	struct rlimit r = { RLIM_INFINITY, RLIM_INFINITY };
	if (setrlimit(RLIMIT_MEMLOCK, &r)) {
		fprintf(stderr, "warning: setrlimit(MEMLOCK) failed: %s\n",
			strerror(errno));
		return -1;
	}
	return 0;
}

static void usage(const char *prog)
{
	printf("Usage: %s [options]\n", prog);
	printf("  -v          verbose (print every free event + libbpf debug)\n");
	printf("  -q          quiet  (don't print events, only heartbeat)\n");
	printf("  -d SEC      run for SEC seconds then exit (0=infinite, default)\n");
	printf("  -h          show this help\n");
	printf("\nRequires: smartmem kernel module loaded, root, BTF.\n");
}

int main(int argc, char **argv)
{
	struct smartmem_bpf *skel = NULL;
	struct ring_buffer *rb = NULL;
	int rc = 1, opt, duration = 0;
	time_t start, last_hb = 0;

	while ((opt = getopt(argc, argv, "vqd:h")) != -1) {
		switch (opt) {
		case 'v': g_verbose = 1; break;
		case 'q': g_quiet = 1; break;
		case 'd': duration = atoi(optarg); break;
		case 'h': default:
			usage(argv[0]);
			return opt == 'h' ? 0 : 1;
		}
	}

	signal(SIGINT, sigh);
	signal(SIGTERM, sigh);

	libbpf_set_print(libbpf_print_fn);
	bump_memlock();

	/* 1. open + load + attach */
	skel = smartmem_bpf__open_and_load();
	if (!skel) {
		fprintf(stderr, "FATAL: failed to open/load BPF skeleton "
				"(need root, BTF, libbpf)\n");
		goto out;
	}
	if (smartmem_bpf__attach(skel)) {
		fprintf(stderr, "FATAL: failed to attach BPF programs\n");
		goto out;
	}
	fprintf(stderr, "memtrace: attached "
			"(kprobe __alloc_pages_noprof + __free_pages)\n");

	/* 2. ringbuf 消费器 */
	rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
			      handle_event, NULL, NULL);
	if (!rb) {
		fprintf(stderr, "FATAL: ring_buffer__new failed: %s\n",
			strerror(errno));
		goto out;
	}

	/* 3. 主循环 — epoll ringbuf + 1Hz 心跳 */
	start = time(NULL);
	push_heartbeat(skel);
	last_hb = start;

	while (!g_exit) {
		int n = ring_buffer__poll(rb, 200 /*ms*/);
		time_t now;

		if (n < 0 && n != -EINTR) {
			fprintf(stderr, "ring_buffer__poll: %d (%s)\n",
				n, strerror(-n));
			break;
		}

		now = time(NULL);
		if (now != last_hb) {
			push_heartbeat(skel);
			last_hb = now;
		}
		if (duration > 0 && (now - start) >= duration)
			break;
	}

	rc = 0;
	fprintf(stderr, "memtrace: stopping. events=%llu allocs=%llu "
			"frees=%llu drops=%llu\n",
		(unsigned long long)g_events_total,
		(unsigned long long)g_alloc_count,
		(unsigned long long)g_free_count,
		(unsigned long long)read_dropped(skel));

out:
	if (rb) ring_buffer__free(rb);
	if (skel) smartmem_bpf__destroy(skel);
	return rc;
}
