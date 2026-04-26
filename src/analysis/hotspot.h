/* 热点识别 */
#ifndef _SMARTMEM_HOTSPOT_H
#define _SMARTMEM_HOTSPOT_H

#include "analysis.h"
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/list.h>

/* 调用栈最大深度 */
#define HOTSPOT_STACK_DEPTH    16

/* 热点哈希表大小 */
#define HOTSPOT_HASH_BITS      8
#define HOTSPOT_HASH_SIZE      (1 << HOTSPOT_HASH_BITS)

/* Top-N 热点数量 */
#define HOTSPOT_TOP_N          10

/* 时间衰减系数（秒） */
#define HOTSPOT_DECAY_INTERVAL  60

/**
 * struct hotspot_entry - 热点条目
 * 按调用栈hash聚合的内存分配统计
 */
struct hotspot_entry {
    u32 stack_hash;                          /* 调用栈hash */
    unsigned long stack_frames[HOTSPOT_STACK_DEPTH]; /* 调用栈帧 */
    int stack_depth;                         /* 栈深度 */

    /* 统计信息 */
    atomic64_t alloc_count;                  /* 分配次数 */
    atomic64_t alloc_pages;                  /* 分配页数 */
    atomic64_t latency_total_ns;             /* 累计延迟(ns) */
    atomic64_t latency_max_ns;               /* 最大延迟(ns) */

    /* 评分 */
    atomic64_t score;                        /* 综合评分 */

    /* 最后更新时间 */
    ktime_t last_update;

    /* 哈希链表 */
    struct hlist_node hnode;
};

/**
 * struct hotspot_top_entry - Top-N 排序条目
 */
struct hotspot_top_entry {
    u32 stack_hash;
    u64 alloc_count;
    u64 alloc_pages;
    u64 latency_avg_ns;
    u64 latency_max_ns;
    u64 score;
    int stack_depth;
    unsigned long stack_frames[HOTSPOT_STACK_DEPTH];
};

/* 热点识别接口 */
int hotspot_init(void);
void hotspot_exit(void);
int hotspot_update(void);

/* 热点数据录入 */
int hotspot_record_alloc(unsigned long *stack, int depth,
                         u32 order, u64 latency_ns);

/* 获取Top-N热点 */
int hotspot_get_top_n(struct hotspot_top_entry *entries, int n);

/* 重置热点数据 */
void hotspot_reset(void);

#endif /* _SMARTMEM_HOTSPOT_H */