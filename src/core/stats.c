/* 统计系统实现 */

#include "stats.h"

/* 全局统计实例 */
static struct smartmem_stats g_stats;

// 定义 per-cpu 统计变量
DEFINE_PER_CPU(struct smartmem_cpu_stats, sm_cpu_stats);

/* 统计初始化 */
int smartmem_stats_init(void)
{
    int cpu;

    pr_info("smartmem: stats initializing...\n");

    atomic64_set(&g_stats.buddy_alloc_count, 0);
    atomic64_set(&g_stats.buddy_free_count, 0);
    atomic64_set(&g_stats.slub_alloc_count, 0);
    atomic64_set(&g_stats.slub_free_count, 0);
    atomic64_set(&g_stats.numa_local_alloc, 0);
    atomic64_set(&g_stats.numa_remote_alloc, 0);

    // 初始化 per-CPU 统计
    for_each_possible_cpu(cpu) {
        struct smartmem_cpu_stats *cpu_stats;
        cpu_stats = per_cpu_ptr(&sm_cpu_stats, cpu);
        cpu_stats->buddy_alloc_count = 0;
		cpu_stats->buddy_free_count = 0;
		cpu_stats->slub_alloc_count = 0;
		cpu_stats->slub_free_count = 0;
    }

    g_stats.initialized = true;

    pr_info("smartmem: stats initialized (with per-CPU support)\n");
    return 0;
}

/* 统计退出 */
void smartmem_stats_exit(void)
{
    pr_info("smartmem: stats exiting...\n");

    // 退出时聚合 per-CPU 统计到全局
    smartmem_stats_aggregate();

    g_stats.initialized = false;

    pr_info("smartmem: stats exited\n");
}

/* 增加Buddy分配计数 */
void smartmem_stats_buddy_alloc_inc(void)
{
    atomic64_inc(&g_stats.buddy_alloc_count);
}

/* 增加Buddy释放计数 */
void smartmem_stats_buddy_free_inc(void)
{
    atomic64_inc(&g_stats.buddy_free_count);
}

/* 增加SLUB分配计数 */
void smartmem_stats_slub_alloc_inc(void)
{
    atomic64_inc(&g_stats.slub_alloc_count);
}

/* 增加SLUB释放计数 */
void smartmem_stats_slub_free_inc(void)
{
    atomic64_inc(&g_stats.slub_free_count);
}

/**
 * 聚合 per-CPU 统计到全局
 */
void smartmem_stats_aggregate(void) 
{
    int cpu;
    u64 buddy_alloc = 0, buddy_free = 0;
    u64 slub_alloc = 0, slub_free = 0;

    // 累加所有 CPU统计
    for_each_possible_cpu(cpu) {
        struct smartmem_cpu_stats *cpu_stats;
        cpu_stats = per_cpu_ptr(&sm_cpu_stats, cpu);
        buddy_alloc += cpu_stats->buddy_alloc_count;
        buddy_free += cpu_stats->buddy_free_count;
        slub_alloc += cpu_stats->slub_alloc_count;
        slub_free += cpu_stats->slub_free_count;
    }

    // 累加到全局统计
    atomic64_add(buddy_alloc, &g_stats.buddy_alloc_count);
	atomic64_add(buddy_free, &g_stats.buddy_free_count);
	atomic64_add(slub_alloc, &g_stats.slub_alloc_count);
	atomic64_add(slub_free, &g_stats.slub_free_count);

    pr_debug("smartmem: stats aggregated (per-CPU -> global)\n");
}

// 读取统计接口
u64 smartmem_stats_get_buddy_alloc(void)
{
    return atomic64_read(&g_stats.buddy_alloc_count);
}

u64 smartmem_stats_get_buddy_free(void)
{
	return atomic64_read(&g_stats.buddy_free_count);
}

u64 smartmem_stats_get_slub_alloc(void)
{
	return atomic64_read(&g_stats.slub_alloc_count);
}

u64 smartmem_stats_get_slub_free(void)
{
	return atomic64_read(&g_stats.slub_free_count);
}

u64 smartmem_stats_get_numa_local(void)
{
	return atomic64_read(&g_stats.numa_local_alloc);
}

u64 smartmem_stats_get_numa_remote(void)
{
	return atomic64_read(&g_stats.numa_remote_alloc);
}

/* 重置所有统计 */
void smartmem_stats_reset(void)
{
    atomic64_set(&g_stats.buddy_alloc_count, 0);
    atomic64_set(&g_stats.buddy_free_count, 0);
    atomic64_set(&g_stats.slub_alloc_count, 0);
    atomic64_set(&g_stats.slub_free_count, 0);
    atomic64_set(&g_stats.numa_local_alloc, 0);
    atomic64_set(&g_stats.numa_remote_alloc, 0);
    
    pr_info("smartmem: stats reset\n");
}

