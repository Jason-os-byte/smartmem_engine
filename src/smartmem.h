/* smartmem.h - 智能内存优化引擎核心头文件 */

#ifndef _SMARTMEM_H
#define _SMARTMEM_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/percpu.h>

/* 版本信息 */
#define SMARTMEM_VERSION       "1.0.0"
#define SMARTMEM_NAME          "smartmem"
#define SMARTMEM_DESC          "Smart Memory Optimization Engine"

/* 功能标志 */
#define SMARTMEM_HOOK_BUDDY   (1 << 0)
#define SMARTMEM_HOOK_SLUB    (1 << 1)
#define SMARTMEM_HOOK_VMA     (1 << 2)
#define SMARTMEM_HOOK_LRU     (1 << 3)
#define SMARTMEM_HOOK_NUMA    (1 << 4)
#define SMARTMEM_HOOK_ALL     0x1F

/* 模块状态 */
enum smartmem_state {
    SMARTMEM_STATE_UNINITIALIZED = 0,
    SMARTMEM_STATE_INITIALIZED,
    SMARTMEM_STATE_READY,
    SMARTMEM_STATE_STOPPING,
    SMARTMEM_STATE_ERROR,
};

/* 全局状态结构 */
struct smartmem_global_state {
    /* 模块状态 */
    enum smartmem_state state;
    bool enabled;

    /* 功能开关 */
    u32 active_hooks;

    /* 统计信息 */
    u64 alloc_count;
    u64 free_count;

    /* 锁 */
    spinlock_t lock;

    /* 引擎指针（指向各个模块） */
    void *engine;
    void *monitor;
    void *analysis;
    void *optimization;
};

/* 外部声明 */
extern struct smartmem_global_state *g_sm_state;

/* 核心接口 */
// int smartmem_init(void) __init;
// void smartmem_exit(void) __exit;
// int smartmem_enable(void);
// int smartmem_disable(void);

#endif /* _SMARTMEM_H */