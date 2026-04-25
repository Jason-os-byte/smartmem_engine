/* multi_gen_lru.c - 多代 LRU 策略实现 */

#include "multi_gen_lru.h"
#include <linux/mm.h>
#include <linux/mmzone.h>

/* 世代定义 */
enum lru_gen {
	LRU_GEN_YOUNG = 0,    /* 年轻代：最近访问的页面 */
	LRU_GEN_OLD,          /* 老年代：较久未访问的页面 */
	LRU_GEN_UNEVICTABLE,  /* 不可回收代 */
	LRU_GEN_MAX,
};

/* 每个世代的统计 */
struct lru_gen_stats {
	atomic64_t page_count;
	atomic64_t accessed_count;
	atomic64_t evicted_count;
};

/* 策略统计 */
struct mglru_stats {
	struct lru_gen_stats gen[LRU_GEN_MAX];
	atomic64_t promote_count;    /* 晋升次数：老年代 → 年轻代 */
	atomic64_t demote_count;     /* 降级次数：年轻代 → 老年代 */
	atomic64_t evict_count;      /* 回收次数 */
};

static struct mglru_stats mglru_stats;

/* 策略配置 */
struct mglru_config {
	int young_gen_ttl_ms;      /* 年轻代生存时间（毫秒） */
	int old_gen_ttl_ms;        /* 老年代生存时间（毫秒） */
	int promote_threshold;     /* 晋升阈值：访问次数 */
};

static struct mglru_config mglru_config = {
	.young_gen_ttl_ms = 5000,
	.old_gen_ttl_ms = 30000,
	.promote_threshold = 2,
};

/* 策略实例 */
static struct lru_strategy mglru_strategy = {
	.name = "multi_gen_lru",
	.version = "1.0",
	.description = "Multi-Generational LRU Strategy",
};

/**
 * 页面访问回调
 * 当页面被访问时调用，更新世代信息
 */
static int mglru_page_accessed(struct page *page)
{
	if (!page)
		return -EINVAL;

	/* 更新年轻代统计 */
	atomic64_inc(&mglru_stats.gen[LRU_GEN_YOUNG].accessed_count);

	pr_debug("mglru: page accessed, pfn=%lu\n", page_to_pfn(page));

	return 0;
}

/**
 * 页面引用回调
 * 检查页面是否应该晋升
 */
static int mglru_page_referenced(struct page *page)
{
	if (!page)
		return -EINVAL;

	/* 更新引用统计 */
	atomic64_inc(&mglru_stats.gen[LRU_GEN_OLD].accessed_count);

	pr_debug("mglru: page referenced, pfn=%lu\n", page_to_pfn(page));

	return 0;
}

/**
 * 页面回收回调
 */
static int mglru_page_evict(struct page *page)
{
	if (!page)
		return -EINVAL;

	atomic64_inc(&mglru_stats.evict_count);
	atomic64_inc(&mglru_stats.gen[LRU_GEN_OLD].evicted_count);

	pr_debug("mglru: page evicted, pfn=%lu\n", page_to_pfn(page));

	return 0;
}

/**
 * 页面晋升回调
 * 老年代 → 年轻代
 */
static int mglru_page_promote(struct page *page)
{
	if (!page)
		return -EINVAL;

	atomic64_inc(&mglru_stats.promote_count);

	pr_debug("mglru: page promoted, pfn=%lu\n", page_to_pfn(page));

	return 0;
}

/**
 * 页面降级回调
 * 年轻代 → 老年代
 */
static int mglru_page_demote(struct page *page)
{
	if (!page)
		return -EINVAL;

	atomic64_inc(&mglru_stats.demote_count);

	pr_debug("mglru: page demoted, pfn=%lu\n", page_to_pfn(page));

	return 0;
}

/**
 * 获取统计信息
 */
void mglru_get_stats(u64 *young_accessed, u64 *old_accessed,
		     u64 *promote, u64 *demote, u64 *evict)
{
	if (young_accessed)
		*young_accessed = atomic64_read(&mglru_stats.gen[LRU_GEN_YOUNG].accessed_count);
	if (old_accessed)
		*old_accessed = atomic64_read(&mglru_stats.gen[LRU_GEN_OLD].accessed_count);
	if (promote)
		*promote = atomic64_read(&mglru_stats.promote_count);
	if (demote)
		*demote = atomic64_read(&mglru_stats.demote_count);
	if (evict)
		*evict = atomic64_read(&mglru_stats.evict_count);
}

/**
 * 重置统计
 */
void mglru_reset_stats(void)
{
	int i;

	for (i = 0; i < LRU_GEN_MAX; i++) {
		atomic64_set(&mglru_stats.gen[i].page_count, 0);
		atomic64_set(&mglru_stats.gen[i].accessed_count, 0);
		atomic64_set(&mglru_stats.gen[i].evicted_count, 0);
	}
	atomic64_set(&mglru_stats.promote_count, 0);
	atomic64_set(&mglru_stats.demote_count, 0);
	atomic64_set(&mglru_stats.evict_count, 0);
}

/**
 * 创建多代 LRU 策略
 */
struct lru_strategy *multi_gen_lru_strategy_create(void)
{
	/* 清空统计 */
	memset(&mglru_stats, 0, sizeof(mglru_stats));

	/* 设置默认配置 */
	mglru_config.young_gen_ttl_ms = 5000;
	mglru_config.old_gen_ttl_ms = 30000;
	mglru_config.promote_threshold = 2;

	/* 设置操作函数 */
	mglru_strategy.ops.page_accessed = mglru_page_accessed;
	mglru_strategy.ops.page_referenced = mglru_page_referenced;
	mglru_strategy.ops.page_evict = mglru_page_evict;
	mglru_strategy.ops.page_promote = mglru_page_promote;
	mglru_strategy.ops.page_demote = mglru_page_demote;

	pr_info("mglru: strategy created\n");
	return &mglru_strategy;
}

/**
 * 销毁多代 LRU 策略
 */
void multi_gen_lru_strategy_destroy(struct lru_strategy *s)
{
	if (!s)
		return;

	memset(&s->ops, 0, sizeof(s->ops));

	pr_info("mglru: strategy destroyed\n");
}

/**
 * 策略初始化
 */
int multi_gen_lru_init(void)
{
	struct lru_strategy *s;
	int ret;

	s = multi_gen_lru_strategy_create();
	if (!s)
		return -ENOMEM;

	ret = lru_strategy_register(s);
	if (ret)
		multi_gen_lru_strategy_destroy(s);

	return ret;
}

/**
 * 策略退出
 */
void multi_gen_lru_exit(void)
{
	struct lru_strategy *s = &mglru_strategy;

	lru_strategy_unregister(s);
	multi_gen_lru_strategy_destroy(s);
}

