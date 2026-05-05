/* trace_monitor.h - tracepoint 监控头文件 */

#ifndef _SMARTMEM_TRACE_MONITOR_H
#define _SMARTMEM_TRACE_MONITOR_H

#include "monitor.h"

int trace_monitor_init(void);
void trace_monitor_exit(void);
int trace_monitor_start(void);
int trace_monitor_stop(void);

u64 trace_monitor_get_event_count(void);
u64 trace_monitor_get_alloc_count(void);
u64 trace_monitor_get_free_count(void);

void trace_monitor_reset_stats(void);

#endif /* _SMARTMEM_TRACE_MONITOR_H */
