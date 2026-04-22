#include "numa_buddy.h"
#include <linux/mm.h>
#include <linux/mmzone.h>

// 策略统计
struct numa_buddy_stats {
    atomic64_t local_alloc_count;
    atomic64_t remote_alloc_count;
    atomic64_t forced_local_count;
};

static struct numa_buddy_stats nb_stats;

// 策略配置
struct numa_buddy_config {
    bool force_local; // 强制本地分配
    int local_threshold; // 本地节点阈值百分比
};

static struct numa_buddy_config nb_config = {
    .force_local = true,
    .local_threshold = 80,
};

// 策略实例
static struct buddy_strategy numa_buddy_strategy = {
    .name = "numa_buddy",
    .version = "1.0",
    .description = "NUMA-aware Buddy Allocator Strategy",
};

/**
 * 获取节点内存使用情况
 * 返回：节点空闲内存百分比
 */
static int get_node_free_percent(int nid)
{
    struct zone *zone;
    unsigned long free = 0, total = 0;
    int i;

    if (nid < 0 || nid >= MAX_NUMNODES)
        return 0;

    for (i = 0; i < MAX_NR_ZONES; i++) {
        zone = &NODE_DATA(nid)->node_zones[i];
        if (!populated_zone(zone))
            continue;
        free += zone_page_state(zone, NR_FREE_PAGES);
        total += zone->present_pages;
    }

    if (total == 0)
        return 0;
    
    return (free * 100) / total;
}

// 选择最佳 NUMA节点
static int numa_buddy_select_node(int preferred_nid, gfp_t gfp_mask)
{
    int local_nid = numa_node_id();
    int free_percent;

    // 如果没有指定偏好节点，使用本地节点
    if (preferred_nid == NUMA_NO_NODE) 
        preferred_nid = local_nid;
    
    // 检查本地节点内存
    free_percent = get_node_free_percent(local_nid);

    // 如果本地节点内存充足（超过阈值），优先使用本地
    if (free_percent >= nb_config.local_threshold) {
        atomic64_inc(&nb_stats.local_alloc_count);
        return local_nid;
    }

    // 如果强制本地分配，即使内存不足也用本地
    if (nb_config.force_local) {
        atomic64_inc(&nb_stats.forced_local_count);
        return local_nid;
    }

    // 否则使用偏好节点（可能是远程）
    if (preferred_nid != local_nid) {
        atomic64_inc(&nb_stats.remote_alloc_count);
    }

    return preferred_nid;
}

// 分配前回调
static int numa_buddy_pre_alloc(int order, gfp_t *gfp_mask, int *nid)
{
	int selected_nid;

	/* 选择最佳节点 */
	selected_nid = numa_buddy_select_node(*nid, *gfp_mask);

	/* 如果选择了本地节点，添加 GFP_THISNODE 标志 */
	if (selected_nid == numa_node_id()) {
		*gfp_mask |= __GFP_THISNODE;
	}

	*nid = selected_nid;

    pr_debug("numa_buddy: pre_alloc order=%d, selected_nid=%d\n",
		 order, selected_nid);

	return 0;
}

// 分配后回调
static int numa_buddy_post_alloc(void *page, int order, gfp_t gfp_mask)
{
	int nid;
	int free_percent;
    struct page *p = (struct page *)page;

	if (!p)
		return -EINVAL;

	nid = page_to_nid(page);

	/* 记录分配结果 */
	pr_debug("numa_buddy: post_alloc page=%p, order=%d, nid=%d\n",
		 page, order, nid);

	/* 检查节点内存状态，如果过低可以触发告警 */
	free_percent = get_node_free_percent(nid);
    if (free_percent < 10) {
		pr_warn("numa_buddy: node %d memory critically low (%d%% free)\n",
			nid, free_percent);
	}

    return 0;
}

// 释放前回调
static int numa_buddy_pre_free(void *page, int order)
{
	if (!page)
		return 0;

	pr_debug("numa_buddy: pre_free page=%p, order=%d\n", page, order);

	return 0;
}

// 释放后回调
static int numa_buddy_post_free(int order)
{
	pr_debug("numa_buddy: post_free order=%d\n", order);
    return 0;
}

// 获取统计信息
void numa_buddy_get_stats(u64 *local, u64 *remote, u64 *forced)
{
	if (local)
		*local = atomic64_read(&nb_stats.local_alloc_count);
	if (remote)
		*remote = atomic64_read(&nb_stats.remote_alloc_count);
	if (forced)
		*forced = atomic64_read(&nb_stats.forced_local_count);
}

// 重置统计信息
void numa_buddy_reset_stats(void)
{
	atomic64_set(&nb_stats.local_alloc_count, 0);
	atomic64_set(&nb_stats.remote_alloc_count, 0);
	atomic64_set(&nb_stats.forced_local_count, 0);
}

// 创建NUAM Buddy策略
struct buddy_strategy *numa_buddy_strategy_create(void)
{
    // 清空统计
    memset(&nb_stats, 0, sizeof(nb_stats));

    // 设置默认配置
    nb_config.force_local = true;
    nb_config.local_threshold = 80;

    // 设置操作函数
    numa_buddy_strategy.ops.pre_alloc = numa_buddy_pre_alloc;
    numa_buddy_strategy.ops.post_alloc = numa_buddy_post_alloc;
    numa_buddy_strategy.ops.pre_free = numa_buddy_pre_free;
    numa_buddy_strategy.ops.post_free = numa_buddy_post_free;
    numa_buddy_strategy.ops.select_node = numa_buddy_select_node;

    pr_info("numa_buddy: strategy created\n");
    return &numa_buddy_strategy;
}

// 销毁 NUMA Buddy 策略
void numa_buddy_strategy_destroy(struct buddy_strategy *s)
{
    if (!s)
        return;

    // 清理操作函数
    memset(&s->ops, 0, sizeof(s->ops));

    pr_info("numa_buddy: strategy destroyed\n");
}

// 策略初始化
int numa_buddy_init(void)
{
    struct buddy_strategy *s;
    int ret;

    s = numa_buddy_strategy_create();
    if (!s)
        return -ENOMEM;
    
    ret = buddy_strategy_register(s);
    if (ret) {
        numa_buddy_strategy_destroy(s);
    }

    return 0;
}

// 策略退出
void numa_buddy_exit(void)
{
    struct buddy_strategy *s = &numa_buddy_strategy;

    buddy_strategy_unregister(s);
    numa_buddy_strategy_destroy(s);
}