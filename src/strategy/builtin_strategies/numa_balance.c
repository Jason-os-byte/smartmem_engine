/* NUMA 负载均衡策略实现 */

#include "numa_balance.h"
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/cpumask.h>

/* 节点负载信息 */
struct numa_node_load {
	atomic64_t alloc_count;
	atomic64_t free_count;
	atomic64_t migrate_in_count;
	atomic64_t migrate_out_count;
	int free_percent;
};

/* 策略统计 */
struct numa_balance_stats {
	struct numa_node_load node_load[MAX_NUMNODES];
	atomic64_t total_migrations;
	atomic64_t imbalance_detect_count;
	atomic64_t balance_count;
};

static struct numa_balance_stats nb_stats;

/* 策略配置 */
struct numa_balance_config {
	int imbalance_threshold;    /* 不平衡阈值百分比 */
	int min_free_percent;       /* 最小空闲百分比 */
	bool auto_balance;          /* 是否自动均衡 */
};

static struct numa_balance_config nb_config = {
	.imbalance_threshold = 20,
	.min_free_percent = 10,
	.auto_balance = true,
};

/* 策略实例 */
static struct numa_strategy numa_balance_strategy = {
	.name = "numa_balance",
	.version = "1.0",
	.description = "NUMA Load Balancing Strategy",
};

/**
 * 获取节点空闲内存百分比
 */
static int get_node_free_percent(int nid)
{
	unsigned long free_pages = 0, total_pages;
	struct zone *zone;
	int i;

	if (nid < 0 || nid >= MAX_NUMNODES || !node_online(nid))
		return 0;

	total_pages = node_present_pages(nid);
	if (total_pages == 0)
		return 0;

	/* 遍历节点的所有 zone，累加空闲页 */
	for (i = 0; i < MAX_NR_ZONES; i++) {
		zone = &NODE_DATA(nid)->node_zones[i];
		if (!populated_zone(zone))
			continue;
		free_pages += zone_page_state(zone, NR_FREE_PAGES);
	}

	return (int)((free_pages * 100) / total_pages);
}




/**
 * 检测 NUMA 不平衡
 * 返回：0=平衡，正数=不平衡程度
 */
static int numa_balance_check_imbalance(void)
{
	int nid;
	int max_free = 0, min_free = 100;
	int imbalance;

	atomic64_inc(&nb_stats.imbalance_detect_count);

	/* 遍历所有在线节点，找出最大和最小空闲率 */
	for_each_online_node(nid) {
		int free_pct = get_node_free_percent(nid);

		if (free_pct > max_free)
			max_free = free_pct;
		if (free_pct < min_free)
			min_free = free_pct;

		nb_stats.node_load[nid].free_percent = free_pct;
	}

	/* 计算不平衡程度 */
	imbalance = max_free - min_free;

	if (imbalance > nb_config.imbalance_threshold) {
		pr_info("numa_balance: imbalance detected (max=%d%%, min=%d%%, gap=%d%%)\n",
			max_free, min_free, imbalance);
		return imbalance;
	}

	return 0;
}

/**
 * 选择目标节点
 * 找到空闲内存最多的节点
 */
static int numa_balance_select_target(int src_nid)
{
	int nid;
	int best_nid = -1;
	int best_free = 0;

	for_each_online_node(nid) {
		int free_pct = get_node_free_percent(nid);

		/* 排除源节点 */
		if (nid == src_nid)
			continue;

		/* 选择空闲率最高的节点 */
		if (free_pct > best_free) {
			best_free = free_pct;
			best_nid = nid;
		}
	}

	if (best_nid >= 0) {
		pr_debug("numa_balance: selected target node %d (%d%% free)\n",
			 best_nid, best_free);
	}

	return best_nid;
}

/**
 * 执行页面迁移
 */
static int numa_balance_migrate(int src_nid, int dst_nid)
{
	if (src_nid < 0 || dst_nid < 0 || src_nid == dst_nid)
		return -EINVAL;

	atomic64_inc(&nb_stats.node_load[src_nid].migrate_out_count);
	atomic64_inc(&nb_stats.node_load[dst_nid].migrate_in_count);
	atomic64_inc(&nb_stats.total_migrations);

	pr_info("numa_balance: migrate node %d -> node %d\n", src_nid, dst_nid);

	/* 实际迁移由内核的 migrate_pages 完成
	 * 这里只记录决策和统计
	 */

	return 0;
}

/**
 * 获取统计信息
 */
void numa_balance_get_stats(u64 *migrations, u64 *imbalance_detect, u64 *balance)
{
	if (migrations)
		*migrations = atomic64_read(&nb_stats.total_migrations);
	if (imbalance_detect)
		*imbalance_detect = atomic64_read(&nb_stats.imbalance_detect_count);
	if (balance)
		*balance = atomic64_read(&nb_stats.balance_count);
}

/**
 * 重置统计
 */
void numa_balance_reset_stats(void)
{
	int nid;

	for (nid = 0; nid < MAX_NUMNODES; nid++) {
		atomic64_set(&nb_stats.node_load[nid].alloc_count, 0);
		atomic64_set(&nb_stats.node_load[nid].free_count, 0);
		atomic64_set(&nb_stats.node_load[nid].migrate_in_count, 0);
		atomic64_set(&nb_stats.node_load[nid].migrate_out_count, 0);
		nb_stats.node_load[nid].free_percent = 0;
	}
	atomic64_set(&nb_stats.total_migrations, 0);
	atomic64_set(&nb_stats.imbalance_detect_count, 0);
	atomic64_set(&nb_stats.balance_count, 0);
}


/**
 * 创建 NUMA 负载均衡策略
 */
struct numa_strategy *numa_balance_strategy_create(void)
{
	/* 清空统计 */
	memset(&nb_stats, 0, sizeof(nb_stats));

	/* 设置默认配置 */
	nb_config.imbalance_threshold = 20;
	nb_config.min_free_percent = 10;
	nb_config.auto_balance = true;

	/* 设置操作函数 */
	numa_balance_strategy.ops.check_imbalance = numa_balance_check_imbalance;
	numa_balance_strategy.ops.select_target_node = numa_balance_select_target;
	numa_balance_strategy.ops.migrate_pages = numa_balance_migrate;

	pr_info("numa_balance: strategy created\n");
	return &numa_balance_strategy;
}

/**
 * 销毁 NUMA 负载均衡策略
 */
void numa_balance_strategy_destroy(struct numa_strategy *s)
{
	if (!s)
		return;

	memset(&s->ops, 0, sizeof(s->ops));

	pr_info("numa_balance: strategy destroyed\n");
}

/**
 * 策略初始化
 */
int numa_balance_init(void)
{
	struct numa_strategy *s;
	int ret;

	s = numa_balance_strategy_create();
	if (!s)
		return -ENOMEM;

	ret = numa_strategy_register(s);
	if (ret)
		numa_balance_strategy_destroy(s);

	return ret;
}

/**
 * 策略退出
 */
void numa_balance_exit(void)
{
	struct numa_strategy *s = &numa_balance_strategy;

	numa_strategy_unregister(s);
	numa_balance_strategy_destroy(s);
}
