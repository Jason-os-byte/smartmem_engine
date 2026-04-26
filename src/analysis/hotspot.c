/* 热点识别实现 */
#include "hotspot.h"
#include <linux/slab.h>
#include <linux/jhash.h>
#include <linux/stacktrace.h>
#include <linux/sort.h>
#include <linux/ktime.h>

/* 热点哈希表 */
static struct hlist_head hotspot_table[HOTSPOT_HASH_SIZE];
static DEFINE_SPINLOCK(hotspot_lock);

/* 当前活跃热点数 */
static atomic_t hotspot_count = ATOMIC_INIT(0);

static bool hotspot_initialized = false;

/* 内存缓存 */
static struct kmem_cache *hotspot_cache = NULL;

/**
 * 计算调用栈hash
 */
static u32 hotspot_stack_hash(unsigned long *frames, int depth)
{
    if (depth <= 0 || depth > HOTSPOT_STACK_DEPTH)
        return 0;

    return jhash2((const u32 *)frames, depth * sizeof(unsigned long) / sizeof(u32),
                  0x5a471234);
}

/**
 * 查找或创建热点条目
 * 调用者需持有 hotspot_lock
 */
static struct hotspot_entry *hotspot_find_or_create(u32 stack_hash,
                                                     unsigned long *frames,
                                                     int depth)
{
    struct hotspot_entry *entry;
    struct hlist_head *head;
    int i;

    head = &hotspot_table[stack_hash & (HOTSPOT_HASH_SIZE - 1)];

    /* 查找已有条目 */
    hlist_for_each_entry(entry, head, hnode) {
        if (entry->stack_hash == stack_hash &&
            entry->stack_depth == depth &&
            memcmp(entry->stack_frames, frames,
                   depth * sizeof(unsigned long)) == 0)
            return entry;
    }

    /* 限制最大热点数 */
    if (atomic_read(&hotspot_count) >= HOTSPOT_HASH_SIZE * 4) {
        pr_warn_ratelimited("smartmem-hotspot: table full, skipping new entry\n");
        return NULL;
    }

    /* 创建新条目 */
    entry = kmem_cache_zalloc(hotspot_cache, GFP_ATOMIC);
    if (!entry)
        return NULL;

    entry->stack_hash = stack_hash;
    entry->stack_depth = depth;
    for (i = 0; i < depth && i < HOTSPOT_STACK_DEPTH; i++)
        entry->stack_frames[i] = frames[i];

    atomic64_set(&entry->alloc_count, 0);
    atomic64_set(&entry->alloc_pages, 0);
    atomic64_set(&entry->latency_total_ns, 0);
    atomic64_set(&entry->latency_max_ns, 0);
    atomic64_set(&entry->score, 0);
    entry->last_update = ktime_get();

    hlist_add_head(&entry->hnode, head);
    atomic_inc(&hotspot_count);

    return entry;
}

/**
 * 计算热点评分
 * 评分 = alloc_count权重 * 频率分 + alloc_pages权重 * 占用分 + latency权重 * 延迟分
 */
static void hotspot_calc_score(struct hotspot_entry *entry)
{
    u64 count_score, pages_score, latency_score;
    u64 alloc_cnt, alloc_pg, lat_total, lat_max;
    ktime_t age;
    u64 decay_factor;

    alloc_cnt = atomic64_read(&entry->alloc_count);
    alloc_pg = atomic64_read(&entry->alloc_pages);
    lat_total = atomic64_read(&entry->latency_total_ns);
    lat_max = atomic64_read(&entry->latency_max_ns);

    /* 频率分：分配次数（对数加权） */
    count_score = int_sqrt(alloc_cnt) * 10;

    /* 占用分：分配页数（线性加权） */
    pages_score = alloc_pg;

    /* 延迟分：平均延迟 + 最大延迟 */
    if (alloc_cnt > 0) {
        latency_score = (lat_total / alloc_cnt) / 1000; /* us */
        latency_score += lat_max / 1000000;              /* ms */
    } else {
        latency_score = 0;
    }

    /* 时间衰减：越久未活跃，分数越低 */
    age = ktime_sub(ktime_get(), entry->last_update);
    decay_factor = ktime_to_ns(age);
    decay_factor = div64_u64(decay_factor,
                             (u64)NSEC_PER_SEC * HOTSPOT_DECAY_INTERVAL);

    if (decay_factor > 0) {
        /* 每个衰减周期分数减半 */
        while (decay_factor-- > 0 && count_score > 1) {
            count_score = count_score * 3 / 4;
            pages_score = pages_score * 3 / 4;
            latency_score = latency_score * 3 / 4;
        }
    }

    atomic64_set(&entry->score, count_score + pages_score + latency_score);
}

/**
 * 记录一次分配到热点
 */
int hotspot_record_alloc(unsigned long *stack, int depth,
                         u32 order, u64 latency_ns)
{
    struct hotspot_entry *entry;
    u32 hash;
    unsigned long flags;

    if (!hotspot_initialized || !stack || depth <= 0)
        return -EINVAL;

    hash = hotspot_stack_hash(stack, depth);

    spin_lock_irqsave(&hotspot_lock, flags);

    entry = hotspot_find_or_create(hash, stack, depth);
    if (entry) {
        atomic64_inc(&entry->alloc_count);
        atomic64_add(1UL << order, &entry->alloc_pages);
        atomic64_add(latency_ns, &entry->latency_total_ns);

        /* 更新最大延迟 */
        if (latency_ns > atomic64_read(&entry->latency_max_ns))
            atomic64_set(&entry->latency_max_ns, latency_ns);

        entry->last_update = ktime_get();
    }

    spin_unlock_irqrestore(&hotspot_lock, flags);

    return 0;
}

/**
 * 更新所有热点评分
 */
int hotspot_update(void)
{
    struct hotspot_entry *entry;
    int i;

    if (!hotspot_initialized)
        return -ENODEV;

    for (i = 0; i < HOTSPOT_HASH_SIZE; i++) {
        hlist_for_each_entry(entry, &hotspot_table[i], hnode)
            hotspot_calc_score(entry);
    }

    return 0;
}

/* 用于排序的比较函数 */
// static struct hotspot_top_entry *sort_array = NULL;

static int hotspot_cmp(const void *a, const void *b)
{
    const struct hotspot_top_entry *ea = a;
    const struct hotspot_top_entry *eb = b;

    if (ea->score < eb->score)
        return 1;
    if (ea->score > eb->score)
        return -1;
    return 0;
}

/**
 * 获取Top-N热点
 */
int hotspot_get_top_n(struct hotspot_top_entry *entries, int n)
{
    struct hotspot_entry *entry;
    int i, count = 0;
    u64 alloc_cnt;

    if (!hotspot_initialized || !entries || n <= 0)
        return -EINVAL;

    /* 先更新评分 */
    hotspot_update();

    /* 收集所有热点到临时数组 */
    for (i = 0; i < HOTSPOT_HASH_SIZE && count < n; i++) {
        hlist_for_each_entry(entry, &hotspot_table[i], hnode) {
            if (count >= n)
                break;

            alloc_cnt = atomic64_read(&entry->alloc_count);
            if (alloc_cnt == 0)
                continue;

            entries[count].stack_hash = entry->stack_hash;
            entries[count].alloc_count = alloc_cnt;
            entries[count].alloc_pages = atomic64_read(&entry->alloc_pages);
            entries[count].latency_max_ns = atomic64_read(&entry->latency_max_ns);
            entries[count].score = atomic64_read(&entry->score);
            entries[count].stack_depth = entry->stack_depth;

            if (alloc_cnt > 0)
                entries[count].latency_avg_ns =
                    atomic64_read(&entry->latency_total_ns) / alloc_cnt;
            else
                entries[count].latency_avg_ns = 0;

            memcpy(entries[count].stack_frames, entry->stack_frames,
                   sizeof(unsigned long) * entry->stack_depth);

            count++;
        }
    }

    /* 按评分排序 */
    if (count > 1)
        sort(entries, count, sizeof(struct hotspot_top_entry),
             hotspot_cmp, NULL);

    return count;
}

/**
 * 重置热点数据
 */
void hotspot_reset(void)
{
    struct hotspot_entry *entry;
    struct hlist_node *tmp;
    int i;
    unsigned long flags;

    spin_lock_irqsave(&hotspot_lock, flags);

    for (i = 0; i < HOTSPOT_HASH_SIZE; i++) {
        hlist_for_each_entry_safe(entry, tmp, &hotspot_table[i], hnode) {
            hlist_del(&entry->hnode);
            kmem_cache_free(hotspot_cache, entry);
        }
    }

    atomic_set(&hotspot_count, 0);

    spin_unlock_irqrestore(&hotspot_lock, flags);
}

/**
 * 热点识别初始化
 */
int hotspot_init(void)
{
    int i;

    pr_info("smartmem: hotspot analysis initializing...\n");

    /* 初始化哈希表 */
    for (i = 0; i < HOTSPOT_HASH_SIZE; i++)
        INIT_HLIST_HEAD(&hotspot_table[i]);

    /* 创建slab缓存 */
    hotspot_cache = kmem_cache_create("smartmem_hotspot",
                                       sizeof(struct hotspot_entry),
                                       0, 0, NULL);
    if (!hotspot_cache) {
        pr_err("smartmem: failed to create hotspot slab cache\n");
        return -ENOMEM;
    }

    atomic_set(&hotspot_count, 0);
    hotspot_initialized = true;

    pr_info("smartmem: hotspot analysis initialized\n");
    return 0;
}

/**
 * 热点识别退出
 */
void hotspot_exit(void)
{
    pr_info("smartmem: hotspot analysis exiting...\n");

    hotspot_reset();

    if (hotspot_cache) {
        kmem_cache_destroy(hotspot_cache);
        hotspot_cache = NULL;
    }

    hotspot_initialized = false;

    pr_info("smartmem: hotspot analysis exited\n");
}