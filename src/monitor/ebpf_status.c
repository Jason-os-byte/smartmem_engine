/* ebpf_status.c - eBPF 用户态加载器状态通道
 *
 * 内核不加载 BPF。用户态 loader (tools/memtrace) 通过写
 * /proc/smartmem/ebpf_status 上报心跳和统计；本文件维护这些状态。
 * 若超过 EBPF_STALE_NS 未收到心跳，则视为 loader 已离线 (active=false)。
 */

#include "ebpf_status.h"
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/printk.h>

/* loader 心跳超时：10 秒未更新则视为离线 */
#define EBPF_STALE_NS  (10ULL * NSEC_PER_SEC)

static DEFINE_SPINLOCK(ebpf_lock);
static struct ebpf_status_snapshot g_status;

int ebpf_status_init(void)
{
	spin_lock_init(&ebpf_lock);
	memset(&g_status, 0, sizeof(g_status));
	pr_info("smartmem: ebpf status channel initialized\n");
	pr_info("smartmem: BPF program: src/bpf/smartmem.bpf.c\n");
	pr_info("smartmem: loader:      tools/memtrace (libbpf)\n");
	return 0;
}

void ebpf_status_exit(void)
{
	spin_lock(&ebpf_lock);
	g_status.active = false;
	spin_unlock(&ebpf_lock);
	pr_info("smartmem: ebpf status channel exited\n");
}

int ebpf_status_update(int pid, u64 events, u64 drops,
		       u64 allocs, u64 frees)
{
	unsigned long flags;

	spin_lock_irqsave(&ebpf_lock, flags);
	g_status.active         = true;
	g_status.loader_pid     = pid;
	g_status.last_update_ns = ktime_get_ns();
	g_status.events_total   = events;
	g_status.events_dropped = drops;
	g_status.alloc_count    = allocs;
	g_status.free_count     = frees;
	spin_unlock_irqrestore(&ebpf_lock, flags);
	return 0;
}

void ebpf_status_get(struct ebpf_status_snapshot *snap)
{
	unsigned long flags;
	u64 now = ktime_get_ns();

	spin_lock_irqsave(&ebpf_lock, flags);
	*snap = g_status;
	if (snap->active && (now - snap->last_update_ns) > EBPF_STALE_NS)
		snap->active = false;
	spin_unlock_irqrestore(&ebpf_lock, flags);
}

bool ebpf_status_is_active(void)
{
	struct ebpf_status_snapshot s;
	ebpf_status_get(&s);
	return s.active;
}
