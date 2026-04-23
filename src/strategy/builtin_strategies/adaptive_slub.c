// 自适应 SLUB 策略实现

#include "adaptive_slub.h"
#include <linux/slab.h>
#include <linux/cpumask.h>

/* 策略统计 */
struct adaptive_slub_stats {
	atomic64_t alloc_count;
	atomic64_t free_count;
	atomic64_t percpu_hit_count;
	atomic64_t percpu_miss_count;
	atomic64_t tune_count;
};

static struct adaptive_slub_stats as_stats;

// 策略配置
struct adaptive_slub_config {
    int target_hit_rate; // 目标命中率百分比
    int tune_interval_ms; // 调优间隔（毫秒）
    int high_watermark; // 高水位：命中率高于此值则缩小缓存
    int low_watermark; // 低水位：命中率低于此值则扩大缓存
};

static struct adaptive_slub_config as_config = {
    .target_hit_rate = 80,
    .tune_interval_ms = 5000,
    .high_watermark = 90,
    .low_watermark = 60,
};

// 缓存命中率跟踪
struct cache_hit_tracker {
    u64 hit_count;
    u64 total_count;
    spinlock_t lock;
};

static DEFINE_PER_CPU(struct cache_hit_tracker, cpu_hit_tracker);

// 策略实例
static struct slub_strategy adaptive_slub_strategy = {
    .name = "adaptive_slub",
    .version = "1.0",
    .description = "Adaptive SLUB Cache Sizing Stratregy",
};

/**
 * 获取当前缓存命中率
 * 返回： 命中率百分比（0-100）
 */
static int get_current_hit_rate(void)
{
    u64 total_hits = 0, total_requests = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		struct cache_hit_tracker *tracker = per_cpu_ptr(&cpu_hit_tracker, cpu);
		total_hits += tracker->hit_count;
		total_requests += tracker->total_count;
	}

    if (total_requests == 0)
		return 100; /* 无请求时视为100%命中 */

	return (int)((total_hits * 100) / total_requests);    

}

// 记录缓存命中
static void record_cache_hit(bool hit)
{
	struct cache_hit_tracker *tracker;

	tracker = this_cpu_ptr(&cpu_hit_tracker);
	spin_lock(&tracker->lock);
	tracker->total_count++;
	if (hit)
		tracker->hit_count++;
	spin_unlock(&tracker->lock);
}

/**
 * 分配前回调
 * 检查是否命中 per-CPU 缓存
 */
static int adaptive_slub_pre_alloc(size_t size, u32 *gfp_mask)
{
	/* 这里无法直接判断是否命中 per-CPU 缓存
	 * 需要在 post_alloc 中根据分配延迟判断
	 * 快速分配 = 缓存命中，慢速分配 = 缓存未命中
	 */

     atomic64_inc(&as_stats.alloc_count);

	pr_debug("adaptive_slub: pre_alloc size=%zu\n", size);

	return 0;
}

/**
 * 分配后回调
 * 根据分配结果判断缓存命中情况
 */
static int adaptive_slub_post_alloc(void *ptr, size_t size)
{
	if (!ptr) {
		/* 分配失败 */
		pr_debug("adaptive_slub: alloc failed, size=%zu\n", size);
		return 0;
	}

	/* 简单判断：如果分配成功，记录命中 */
	record_cache_hit(true);
	atomic64_inc(&as_stats.percpu_hit_count);

    pr_debug("adaptive_slub: post_alloc ptr=%p, size=%zu\n", ptr, size);

	return 0;
}

/**
 * 释放前回调
 */
static int adaptive_slub_pre_free(void *ptr)
{
	if (!ptr)
		return 0;

	atomic64_inc(&as_stats.free_count);

	pr_debug("adaptive_slub: pre_free ptr=%p\n", ptr);

	return 0;
}

/**
 * 释放后回调
 * 检查命中率并决定是否调优
 */
static int adaptive_slub_post_free(void)
{
	int hit_rate;
	u64 total;

	/* 获取总分配次数 */
	total = atomic64_read(&as_stats.alloc_count);

	/* 每隔一定次数检查一次命中率 */
	if (total % 1000 != 0)
		return 0;
    
    hit_rate = get_current_hit_rate();

	pr_debug("adaptive_slub: hit_rate=%d%%\n", hit_rate);

	/* 根据命中率决定调优方向 */
	if (hit_rate < as_config.low_watermark) {
		/* 命中率过低，建议扩大 per-CPU 缓存 */
		pr_info("adaptive_slub: low hit rate (%d%%), suggesting cache increase\n",
			hit_rate);
		atomic64_inc(&as_stats.tune_count);
	} else if (hit_rate > as_config.high_watermark) {
		/* 命中率过高，可以缩小 per-CPU 缓存释放内存 */
		pr_info("adaptive_slub: high hit rate (%d%%), suggesting cache decrease\n",
			hit_rate);
		atomic64_inc(&as_stats.tune_count);
	}

	return 0;
}

/**
 * 获取统计信息
 */
void adaptive_slub_get_stats(u64 *alloc, u64 *free, u64 *hit, u64 *miss, u64 *tune)
{
	if (alloc)
		*alloc = atomic64_read(&as_stats.alloc_count);
	if (free)
		*free = atomic64_read(&as_stats.free_count);
	if (hit)
		*hit = atomic64_read(&as_stats.percpu_hit_count);
	if (miss)
		*miss = atomic64_read(&as_stats.percpu_miss_count);
	if (tune)
		*tune = atomic64_read(&as_stats.tune_count);
}

/**
 * 重置统计
 */
void adaptive_slub_reset_stats(void)
{
	int cpu;

	atomic64_set(&as_stats.alloc_count, 0);
	atomic64_set(&as_stats.free_count, 0);
	atomic64_set(&as_stats.percpu_hit_count, 0);
	atomic64_set(&as_stats.percpu_miss_count, 0);
	atomic64_set(&as_stats.tune_count, 0);

    /* 重置 per-CPU 跟踪器 */
	for_each_possible_cpu(cpu) {
		struct cache_hit_tracker *tracker = per_cpu_ptr(&cpu_hit_tracker, cpu);
		spin_lock(&tracker->lock);
		tracker->hit_count = 0;
		tracker->total_count = 0;
		spin_unlock(&tracker->lock);
	}
}

/**
 * 创建自适应 SLUB 策略
 */
struct slub_strategy *adaptive_slub_strategy_create(void)
{
	int cpu;

    pr_info("adaptive_slub: strategy creatint...\n");

	/* 清空统计 */
	memset(&as_stats, 0, sizeof(as_stats));

	/* 初始化 per-CPU 跟踪器 */
	for_each_possible_cpu(cpu) {
		struct cache_hit_tracker *tracker = per_cpu_ptr(&cpu_hit_tracker, cpu);
		spin_lock_init(&tracker->lock);
		tracker->hit_count = 0;
		tracker->total_count = 0;
	}

    /* 设置默认配置 */
	as_config.target_hit_rate = 80;
	as_config.tune_interval_ms = 5000;
	as_config.high_watermark = 90;
	as_config.low_watermark = 60;

	/* 设置操作函数 */
	adaptive_slub_strategy.ops.pre_alloc = adaptive_slub_pre_alloc;
	adaptive_slub_strategy.ops.post_alloc = adaptive_slub_post_alloc;
	adaptive_slub_strategy.ops.pre_free = adaptive_slub_pre_free;
	adaptive_slub_strategy.ops.post_free = adaptive_slub_post_free;

	pr_info("adaptive_slub: strategy created\n");
	return &adaptive_slub_strategy;
}

/**
 * 销毁自适应 SLUB 策略
 */
void adaptive_slub_strategy_destroy(struct slub_strategy *s)
{
	if (!s)
		return;

	memset(&s->ops, 0, sizeof(s->ops));

	pr_info("adaptive_slub: strategy destroyed\n");
}

// 策略初始化
int adaptive_slub_init(void)
{
    struct slub_strategy *s;
    int ret;

    s = adaptive_slub_strategy_create();
    if (!s)
        return -ENOMEM;
    
    ret = slub_strategy_register(s);
    if (ret) {
        adaptive_slub_strategy_destroy(s);
    }

    return 0;
}

// 策略退出
void adaptive_slub_exit(void)
{
    struct slub_strategy *s = &adaptive_slub_strategy;

    slub_strategy_unregister(s);
    adaptive_slub_strategy_destroy(s);
}

