/* 瓶颈分析 */

#ifndef _SMARTMEM_BOTTLENECK_H
#define _SMARTMEM_BOTTLENECK_H

#include "analysis.h"

/* 瓶颈类型 */
enum bottleneck_type {
    BOTTLENECK_NONE = 0,
    BOTTLENECK_SLOW_ALLOC,       /* 分配延迟过高 */
    BOTTLENECK_HIGH_FRAGMENT,    /* 内存碎片严重 */
    BOTTLENECK_NUMA_IMBALANCE,   /* NUMA 不平衡 */
    BOTTLENECK_LOW_SLAB_HIT,     /* SLAB 命中率低 */
    BOTTLENECK_OOM_RISK,         /* OOM 风险 */
    BOTTLENECK_TYPE_MAX,
};

/* 瓶颈严重级别 */
enum bottleneck_severity {
    SEVERITY_LOW = 0,
    SEVERITY_MEDIUM,
    SEVERITY_HIGH,
    SEVERITY_CRITICAL,
};

/* 瓶颈条目 */
struct bottleneck_entry {
    enum bottleneck_type type;
    enum bottleneck_severity severity;
    u64 value;                   /* 当前值 */
    u64 threshold;               /* 阈值 */
    char description[128];       /* 描述 */
    ktime_t detected_time;       /* 检测时间 */
};

/* 最大瓶颈数 */
#define MAX_BOTTLENECKS  8

/* 瓶颈分析接口 */
int bottleneck_init(void);
void bottleneck_exit(void);
int bottleneck_update(void);

/* 获取检测到的瓶颈 */
int bottleneck_get_entries(struct bottleneck_entry *entries, int max);

/* 配置阈值 */
void bottleneck_set_threshold(enum bottleneck_type type, u64 threshold);
u64 bottleneck_get_threshold(enum bottleneck_type type);

#endif /* _SMARTMEM_BOTTLENECK_H */