/* ebpf_monitor.h - eBPF 监控头文件 */

#ifndef _SMARTMEM_EBPF_MONITOR_H
#define _SMARTMEM_EBPF_MONITOR_H

#include "monitor.h"

int ebpf_monitor_init(void);
void ebpf_monitor_exit(void);
int ebpf_monitor_start(void);
int ebpf_monitor_stop(void);
bool ebpf_monitor_is_active(void);

#endif /* _SMARTMEM_EBPF_MONITOR_H */