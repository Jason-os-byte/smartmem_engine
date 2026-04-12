/* 统计系统实现 */

#include "stats.h"

/* 全局统计实例 */
static struct smartmem_stats g_stats;

/* 统计初始化 */
int smartmem_stats_init(void)
{
    pr_info("smartmem: stats initializing...\n");

    atomic64_set(&g_stats.buddy_alloc_count, 0);
    atomic64_set(&g_stats.buddy_free_count, 0);
    atomic64_set(&g_stats.slub_alloc_count, 0);
    atomic64_set(&g_stats.slub_free_count, 0);
    atomic64_set(&g_stats.numa_local_alloc, 0);
    atomic64_set(&g_stats.numa_remote_alloc, 0);

    g_stats.initialized = true;

    pr_info("smartmem: stats initialized\n");
    return 0;
}

/* 统计退出 */
void smartmem_stats_exit(void)
{
    pr_info("smartmem: stats exiting...\n");

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

