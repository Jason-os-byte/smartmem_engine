/* 监控系统 */
#ifndef _SMARTMEM_MONITOR_H
#define _SMARTMEM_MONITOR_H

#include "smartmem.h"

/**
 * 监控系统
 */
struct smartmem_monitor {
    bool ebpf_enabled;
    bool trace_enabled;
    bool initialized;
};

/* 监控接口 */
int smartmem_monitor_init(void);
void smartmem_monitor_exit(void);
int smartmem_monitor_start(void);
int smartmem_monitor_stop(void);

#endif /* _SMARTMEM_MONITOR_H */