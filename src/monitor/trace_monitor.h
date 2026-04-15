/* tracepoint监控 */

#ifndef _SMARTMEM_TRACE_MONITOR_H
#define _SMARTMEM_TRACE_MONITOR_H

#include "monitor.h"

/* tracepoint监控接口 */
int trace_monitor_init(void);
void trace_monitor_exit(void);
int trace_monitor_start(void);
int trace_monitor_stop(void);

#endif /* _SMARTMEM_TRACE_MONITOR_H */