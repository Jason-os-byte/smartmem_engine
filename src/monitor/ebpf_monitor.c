/* ebpf_monitor.c - eBPF 监控协调层
 *
 * 真正的 eBPF 程序: src/bpf/smartmem.bpf.c
 * 用户空间加载器:  tools/memstat (libbpf skeleton)
 * 内核模块职责:    跟踪 eBPF 状态，提供内核侧协调
 */

#include "ebpf_monitor.h"

static bool ebpf_active = false;

int ebpf_monitor_init(void)
{
	pr_info("smartmem: eBPF monitor initializing...\n");
	pr_info("smartmem: BPF program: src/bpf/smartmem.bpf.c\n");
	pr_info("smartmem: Load with: tools/memstat (libbpf)\n");
	ebpf_active = false;
	pr_info("smartmem: eBPF monitor initialized\n");
	return 0;
}

void ebpf_monitor_exit(void)
{
	pr_info("smartmem: eBPF monitor exiting...\n");
	ebpf_active = false;
	pr_info("smartmem: eBPF monitor exited\n");
}

int ebpf_monitor_start(void)
{
	pr_info("smartmem: eBPF start (requires userspace: tools/memstat)\n");
	ebpf_active = true;
	return 0;
}

int ebpf_monitor_stop(void)
{
	pr_info("smartmem: eBPF monitor stopped\n");
	ebpf_active = false;
	return 0;
}

bool ebpf_monitor_is_active(void)
{
	return ebpf_active;
}
