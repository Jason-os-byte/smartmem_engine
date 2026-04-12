/* 统计系统 */
#ifndef _SMARTMEM_STATS_H
#define _SMARTMEM_STATS_H

#include "smartmem.h"

/* 统计结构 */
struct smartmem_stats {
    /* Buddy统计 */
    atomic64_t buddy_alloc_count;
    atomic64_t buddy_free_count;

    /* SLUB统计 */
    atomic64_t slub_alloc_count;
    atomic64_t slub_free_count;

    /* NUMA统计 */
    atomic64_t numa_local_alloc;
    atomic64_t numa_remote_alloc;

    /* 模块状态 */
    bool initialized;
};

/* per-CPU统计 */
struct smartmem_cpu_stats {
    u64 buddy_alloc_count;
    u64 slub_alloc_count;
};

/* 统计接口 */
int smartmem_stats_init(void);
void smartmem_stats_exit(void);
void smartmem_stats_buddy_alloc_inc(void);
void smartmem_stats_buddy_free_inc(void);
void smartmem_stats_slub_alloc_inc(void);
void smartmem_stats_slub_free_inc(void);
void smartmem_stats_reset(void);

#endif /* _SMARTMEM_STATS_H */