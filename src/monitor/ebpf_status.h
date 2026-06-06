/* ebpf_status.h - 内核侧 eBPF 用户态加载器状态通道
 *
 * 真正的 BPF 程序由用户态 loader 加载（tools/memtrace, libbpf）。
 * 内核模块本身不加载 BPF，只通过本通道接收 loader 心跳与统计，
 * 在 /proc/smartmem/ebpf_status 暴露给用户。
 */
#ifndef _SMARTMEM_EBPF_STATUS_H
#define _SMARTMEM_EBPF_STATUS_H

#include <linux/types.h>

struct ebpf_status_snapshot {
	bool   active;        /* loader 是否在线 */
	int    loader_pid;    /* loader 进程 PID（0 = 未知） */
	u64    last_update_ns;/* 上次心跳时间（ktime） */
	u64    events_total;  /* 累计上报事件数 */
	u64    events_dropped;/* ringbuf 满丢弃数 */
	u64    alloc_count;   /* alloc 事件数 */
	u64    free_count;    /* free 事件数 */
};

int  ebpf_status_init(void);
void ebpf_status_exit(void);

/* loader 写入心跳: "<pid> <events> <drops> <allocs> <frees>" */
int  ebpf_status_update(int pid, u64 events, u64 drops,
			u64 allocs, u64 frees);

/* 查询当前快照（若超过 STALE_NS 未更新则 active=false） */
void ebpf_status_get(struct ebpf_status_snapshot *snap);
bool ebpf_status_is_active(void);

#endif /* _SMARTMEM_EBPF_STATUS_H */
