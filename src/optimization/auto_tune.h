/* 自动调优 */
#ifndef _SMARTMEM_AUTO_TUNE_H
#define _SMARTMEM_AUTO_TUNE_H

#include "optimization.h"
#include "bottleneck.h"

/* 调优动作类型 */
enum tune_action {
    TUNE_ACTION_NONE = 0,
    TUNE_ACTION_COMPACT,          /* 触发内存整理 */
    TUNE_ACTION_ADJUST_WATERMARK, /* 调整水位线 */
    TUNE_ACTION_NUMA_MIGRATE,     /* NUMA 页面迁移 */
    TUNE_ACTION_ADJUST_SLAB,      /* 调整 SLAB 缓存 */
    TUNE_ACTION_TYPE_MAX,
};

/* 调优历史记录 */
struct tune_history {
    enum tune_action action;
    enum bottleneck_type trigger;
    ktime_t timestamp;
    int result;                   /* 0=成功, 负数=失败 */
    char description[64];
};

/* 调优统计 */
struct tune_stats {
    atomic64_t total_tune_count;
    atomic64_t compact_count;
    atomic64_t watermark_count;
    atomic64_t numa_migrate_count;
    atomic64_t slab_adjust_count;
    atomic64_t fail_count;
};

/* 最大历史记录 */
#define MAX_TUNE_HISTORY  16

/* 自动调优接口 */
int auto_tune_init(void);
void auto_tune_exit(void);
int auto_tune_start(void);
int auto_tune_stop(void);

/* 执行调优检查 */
int auto_tune_check(void);

/* 手动触发调优 */
int auto_tune_trigger(enum tune_action action);

/* 获取调优统计 */
void auto_tune_get_stats(struct tune_stats *stats);

/* 获取调优历史 */
int auto_tune_get_history(struct tune_history *entries, int max);

/* 重置统计 */
void auto_tune_reset_stats(void);

#endif /* _SMARTMEM_AUTO_TUNE_H */
