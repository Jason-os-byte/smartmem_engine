/* smartmem.bpf.c - SmartMemEngine BPF Program
 *
 * CO-RE (Compile Once - Run Everywhere)
 * Requires: Linux 5.2+ with BTF support
 */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#define SM_BPF_VERSION 1

/* 事件类型 */
enum sm_event_type {
	SM_EVENT_BUDDY_ALLOC = 1,
	SM_EVENT_BUDDY_FREE,
	SM_EVENT_PAGE_FAULT,
};

/* 事件数据结构 */
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

/* Ring buffer */
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} events SEC(".maps");

/* Stack trace storage */
struct {
	__uint(type, BPF_MAP_TYPE_STACK_TRACE);
	__uint(max_entries, 4096);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, 127 * sizeof(__u64));
} stack_traces SEC(".maps");

/* Per-CPU statistics */
struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 4);
	__type(key, __u32);
	__type(value, __u64);
} stats SEC(".maps");

enum stat_key {
	STAT_ALLOC_CNT = 0,
	STAT_FREE_CNT,
	STAT_EVENT_DROPPED,
};

char LICENSE[] SEC("license") = "GPL";

static __always_inline void update_stat(__u32 key, __u64 delta)
{
	__u64 *count = bpf_map_lookup_elem(&stats, &key);
	if (count)
		__sync_fetch_and_add(count, delta);
}

/* ============================================
 * Buddy Allocation Monitoring
 * ============================================ */

struct alloc_pages_args {
	__u64 start_ns;
	__u32 order;
	__u32 gfp_mask;
};

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct alloc_pages_args);
} alloc_args SEC(".maps");

SEC("kprobe/__alloc_pages_noprof")
int BPF_KPROBE(trace_alloc_pages, gfp_t gfp_mask, unsigned int order)
{
	__u32 key = 0;
	struct alloc_pages_args *args;

	args = bpf_map_lookup_elem(&alloc_args, &key);
	if (!args)
		return 0;

	args->start_ns = bpf_ktime_get_ns();
	args->order = order;
	args->gfp_mask = gfp_mask;

	update_stat(STAT_ALLOC_CNT, 1);

	return 0;
}

SEC("kretprobe/__alloc_pages_noprof")
int BPF_KRETPROBE(trace_alloc_pages_ret, struct page *page)
{
	__u32 key = 0;
	struct alloc_pages_args *args;
	struct sm_event *e;
	__u64 end_ns;

	args = bpf_map_lookup_elem(&alloc_args, &key);
	if (!args)
		return 0;

	end_ns = bpf_ktime_get_ns();

	if (!page)
		return 0;

	e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
	if (!e) {
		update_stat(STAT_EVENT_DROPPED, 1);
		return 0;
	}

	e->type = SM_EVENT_BUDDY_ALLOC;
	e->pid = bpf_get_current_pid_tgid() >> 32;
	e->timestamp = args->start_ns;
	e->buddy_alloc.order = args->order;
	e->buddy_alloc.gfp_mask = args->gfp_mask;
	e->buddy_alloc.latency_ns = end_ns - args->start_ns;

	e->stack_id = bpf_get_stackid(ctx, &stack_traces,
				       BPF_F_FAST_STACK_CMP | BPF_F_USER_STACK);

	bpf_ringbuf_submit(e, 0);

	return 0;
}

SEC("kprobe/__free_pages")
int BPF_KPROBE(trace_free_pages, struct page *page, unsigned int order)
{
	struct sm_event *e;

	update_stat(STAT_FREE_CNT, 1);

	e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
	if (!e) {
		update_stat(STAT_EVENT_DROPPED, 1);
		return 0;
	}

	e->type = SM_EVENT_BUDDY_FREE;
	e->pid = bpf_get_current_pid_tgid() >> 32;
	e->timestamp = bpf_ktime_get_ns();
	e->buddy_free.order = order;

	bpf_ringbuf_submit(e, 0);

	return 0;
}
