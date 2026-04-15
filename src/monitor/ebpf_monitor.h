/* eBPF监控 */
#ifndef _SMARTMEM_EBPF_MONITOR_H
#define _SMARTMEM_EBPF_MONITOR_H

#include "monitor.h"

/* eBPF监控接口 */
int ebpf_monitor_init(void);
void ebpf_monitor_exit(void);
int ebpf_monitor_load(void);
int ebpf_monitor_unload(void);
int ebpf_monitor_start(void);
int ebpf_monitor_stop(void);


#endif /* _SMARTMEM_EBPF_MONITOR_H */