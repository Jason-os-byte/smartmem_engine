/* 根因分析 */

#ifndef _SMARTMEM_ROOT_CAUSE_H
#define _SMARTMEM_ROOT_CAUSE_H

#include "analysis.h"
#include "bottleneck.h"

/* 根因类型 */
enum root_cause_type {
    ROOT_CAUSE_NONE = 0,
    ROOT_CAUSE_KSWAPD_PRESSURE,   /* kswapd 回收压力大 */
    ROOT_CAUSE_DIRECT_RECLAIM,    /* 直接回收频繁 */
    ROOT_CAUSE_COMPACT_FAIL,      /* 内存整理失败 */
    ROOT_CAUSE_SLAB_BLOAT,        /* SLAB 缓存膨胀 */
    ROOT_CAUSE_MISPLACED_NUMA,    /* NUMA 放置不当 */
    ROOT_CAUSE_TYPE_MAX,
};

/* 根因条目 */
struct root_cause_entry {
    enum root_cause_type type;
    enum bottleneck_type related_bottleneck;
    u64 confidence;               /* 置信度 0-100 */
    char description[128];
    char suggestion[128];         /* 优化建议 */
};

/* 最大根因数 */
#define MAX_ROOT_CAUSES  4

/* 根因分析接口 */
int root_cause_init(void);
void root_cause_exit(void);
int root_cause_update(void);

/* 获取根因分析结果 */
int root_cause_get_entries(struct root_cause_entry *entries, int max);

#endif /* _SMARTMEM_ROOT_CAUSE_H */