/* 预测模型实现
 * 基于历史采样的线性趋势预测：
 * 1. 定期采样内存状态（空闲率、分配速率、释放速率）
 * 2. 用线性回归预测空闲率趋势
 * 3. 预测 OOM 风险和内存耗尽时间
 */

#include "predictive.h"
#include "stats.h"
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>

static bool predictive_initialized = false;
static bool predictive_running = false;

/* 统计 */
static struct predict_stats pd_stats;

/* 历史采样环形缓冲 */
static struct predict_sample pd_samples[PREDICT_SAMPLE_COUNT];
static int pd_sample_idx = 0;
static int pd_sample_count = 0;
static DEFINE_SPINLOCK(pd_sample_lock);

/* 上一次采样时的统计快照，用于计算速率 */
static u64 last_alloc_count = 0;
static u64 last_free_count = 0;
static ktime_t last_sample_time;

/* 定期采样工作队列 */
static struct delayed_work pd_work;

/**
 * 获取当前空闲内存百分比
 */
static int get_free_pct(void)
{
    unsigned long total = totalram_pages();
    if (total == 0)
        return 0;
    return (int)((global_zone_page_state(NR_FREE_PAGES) * 100) / total);
}

/**
 * 添加采样点
 */
static void add_sample(int free_pct, u64 alloc_rate, u64 free_rate)
{
    struct predict_sample *s;
    unsigned long flags;

    spin_lock_irqsave(&pd_sample_lock, flags);

    s = &pd_samples[pd_sample_idx];
    s->timestamp = ktime_get();
    s->free_pct = free_pct;
    s->alloc_rate = alloc_rate;
    s->free_rate = free_rate;

    pd_sample_idx = (pd_sample_idx + 1) % PREDICT_SAMPLE_COUNT;
    if (pd_sample_count < PREDICT_SAMPLE_COUNT)
        pd_sample_count++;

    spin_unlock_irqrestore(&pd_sample_lock, flags);

    atomic64_inc(&pd_stats.sample_count);
}

/**
 * 采样工作函数
 */
static void predictive_work_fn(struct work_struct *work)
{
    int free_pct;
    u64 alloc_now, free_now;
    u64 alloc_rate = 0, free_rate = 0;
    ktime_t now;
    s64 elapsed_ns;

    if (!predictive_running)
        return;

    free_pct = get_free_pct();
    alloc_now = smartmem_stats_get_buddy_alloc();
    free_now = smartmem_stats_get_buddy_free();
    now = ktime_get();

    /* 计算速率（页/秒） */
    elapsed_ns = ktime_to_ns(ktime_sub(now, last_sample_time));
    if (elapsed_ns > 0 && last_sample_time > 0) {
        u64 elapsed_sec = elapsed_ns / NSEC_PER_SEC;
        if (elapsed_sec > 0) {
            alloc_rate = (alloc_now - last_alloc_count) / elapsed_sec;
            free_rate = (free_now - last_free_count) / elapsed_sec;
        }
    }

    last_alloc_count = alloc_now;
    last_free_count = free_now;
    last_sample_time = now;

    add_sample(free_pct, alloc_rate, free_rate);

    /* 重新调度 */
    if (predictive_running)
        schedule_delayed_work(&pd_work,
                              msecs_to_jiffies(PREDICT_SAMPLE_INTERVAL * 1000));
}

/**
 * 线性回归预测
 * 用最近 N 个采样点拟合直线 y = a + b*x
 * 预测未来空闲率趋势
 *
 * slope 和 intercept 用100倍精度存储（slope=150 表示 1.50%/采样）
 */
static int linear_predict(int *slope, int *intercept)
{
    struct predict_sample samples[PREDICT_SAMPLE_COUNT];
    int count, i;
    unsigned long flags;
    s64 sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    s64 denom, num;
    int start_idx;
    s64 x_val;

    spin_lock_irqsave(&pd_sample_lock, flags);
    count = pd_sample_count;
    for (i = 0; i < count; i++)
        samples[i] = pd_samples[i];
    spin_unlock_irqrestore(&pd_sample_lock, flags);

    if (count < 3)
        return -ENOENT;

    /* x = 采样序号(0,1,2,...)，y = free_pct * 100 (放大100倍保精度) */
    start_idx = (pd_sample_idx - count + PREDICT_SAMPLE_COUNT) %
                PREDICT_SAMPLE_COUNT;

    for (i = 0; i < count; i++) {
        x_val = i;
        sum_x += x_val;
        sum_y += (s64)samples[(start_idx + i) % PREDICT_SAMPLE_COUNT].free_pct * 100;
        sum_xy += x_val * (s64)samples[(start_idx + i) % PREDICT_SAMPLE_COUNT].free_pct * 100;
        sum_x2 += x_val * x_val;
    }

    /* y = intercept + slope * x, slope/intercept 都是100倍精度 */
    denom = (s64)count * sum_x2 - sum_x * sum_x;
    if (denom == 0)
        return -EINVAL;

    num = (s64)count * sum_xy - sum_x * sum_y;
    *slope = (int)(num / denom);
    *intercept = (int)((sum_y * 100 - (s64)(*slope) * sum_x) / ((s64)count * 100));

    return 0;
}

/**
 * 执行预测
 */
int predictive_predict(struct prediction_result *result)
{
    int slope, intercept;
    int current_free;
    int predicted_free;
    int ret;

    if (!predictive_initialized || !result)
        return -EINVAL;

    memset(result, 0, sizeof(*result));

    current_free = get_free_pct();

    /* 至少需要3个采样点 */
    if (pd_sample_count < 3) {
        result->type = PREDICT_NONE;
        result->predicted_free_pct = current_free;
        snprintf(result->description, sizeof(result->description),
                 "Insufficient data: %d samples (need 3)", pd_sample_count);
        return 0;
    }

    ret = linear_predict(&slope, &intercept);
    if (ret) {
        result->type = PREDICT_NONE;
        result->predicted_free_pct = current_free;
        snprintf(result->description, sizeof(result->description),
                 "Prediction failed: no clear trend");
        return 0;
    }

    /* slope/intercept 是100倍精度，还原为实际百分比 */
    /* slope=150 表示每个采样间隔变化 1.50% */
    /* intercept 是 x=0 时的预测值，也是100倍精度 */

    /* 预测 PREDICT_SAMPLE_COUNT 个间隔后的空闲率（还原精度） */
    predicted_free = (intercept + slope * (pd_sample_count + PREDICT_SAMPLE_COUNT)) / 100;

    /* 限制范围 */
    if (predicted_free < 0)
        predicted_free = 0;
    if (predicted_free > 100)
        predicted_free = 100;

    result->predicted_free_pct = predicted_free;

    /* slope 百分比：slope/100 = 每5秒变化百分比 */
    /* slope_per_pct = slope / 100，用于判断和显示 */

    /* 预测到达危险水平的时间 */
    if (slope < 0 && current_free > 5) {
        /* 空闲率在下降，计算降到 5% 需要多少步 */
        int steps_to_danger = 0;
        int tmp;

        while (steps_to_danger < 1000) {
            steps_to_danger++;
            tmp = (intercept + slope * (pd_sample_count + steps_to_danger)) / 100;
            if (tmp <= 5)
                break;
        }

        result->estimated_time_sec = (u64)steps_to_danger *
                                     PREDICT_SAMPLE_INTERVAL;
    }

    /* 根据趋势判断预测类型
     * slope < -50 表示每采样间隔下降 >0.5%
     * slope < -200 表示每采样间隔下降 >2%
     */
    if (predicted_free <= 0) {
        result->type = PREDICT_MEMORY_EXHAUST;
        result->severity = PREDICT_SEV_HIGH;
        result->confidence = min(100ULL, (u64)(-slope) / 10);
        snprintf(result->description, sizeof(result->description),
                 "Memory exhaustion predicted: %d%% -> %d%% in %llus (slope=%d.%02d%%/sample)",
                 current_free, predicted_free,
                 result->estimated_time_sec,
                 slope / 100, abs(slope) % 100);
        atomic64_inc(&pd_stats.oom_predict_count);
    } else if (predicted_free <= 5) {
        result->type = PREDICT_OOM_RISK;
        result->severity = PREDICT_SEV_HIGH;
        result->confidence = min(100ULL, (u64)(-slope) / 10);
        snprintf(result->description, sizeof(result->description),
                 "OOM risk predicted: %d%% -> %d%% in %llus (slope=%d.%02d%%/sample)",
                 current_free, predicted_free,
                 result->estimated_time_sec,
                 slope / 100, abs(slope) % 100);
        atomic64_inc(&pd_stats.oom_predict_count);
    } else if (predicted_free <= 10 || slope < -50) {
        result->type = PREDICT_PRESSURE_INCREASE;
        result->severity = PREDICT_SEV_MEDIUM;
        result->confidence = min(80ULL, (u64)(-slope) / 20);
        snprintf(result->description, sizeof(result->description),
                 "Memory pressure increasing: %d%% -> %d%% (slope=%d.%02d%%/sample)",
                 current_free, predicted_free,
                 slope / 100, abs(slope) % 100);
    } else {
        result->type = PREDICT_NONE;
        result->severity = PREDICT_SEV_LOW;
        result->confidence = 60;
        if (slope > 0) {
            snprintf(result->description, sizeof(result->description),
                     "Memory stable: %d%% -> %d%% (improving, slope=+%d.%02d%%/sample)",
                     current_free, predicted_free,
                     slope / 100, slope % 100);
        } else if (slope < 0) {
            snprintf(result->description, sizeof(result->description),
                     "Memory stable: %d%% -> %d%% (declining, slope=%d.%02d%%/sample)",
                     current_free, predicted_free,
                     slope / 100, abs(slope) % 100);
        } else {
            snprintf(result->description, sizeof(result->description),
                     "Memory stable: %d%% -> %d%% (flat)", current_free, predicted_free);
        }
    }

    atomic64_inc(&pd_stats.prediction_count);
    return 0;
}

/**
 * 获取统计
 */
void predictive_get_stats(struct predict_stats *stats)
{
    if (stats)
        *stats = pd_stats;
}

/**
 * 重置
 */
void predictive_reset(void)
{
    unsigned long flags;

    atomic64_set(&pd_stats.sample_count, 0);
    atomic64_set(&pd_stats.prediction_count, 0);
    atomic64_set(&pd_stats.oom_predict_count, 0);

    spin_lock_irqsave(&pd_sample_lock, flags);
    pd_sample_idx = 0;
    pd_sample_count = 0;
    spin_unlock_irqrestore(&pd_sample_lock, flags);

    last_alloc_count = 0;
    last_free_count = 0;
    last_sample_time = 0;
}

/**
 * 预测模型初始化
 */
int predictive_init(void)
{
    pr_info("smartmem: predictive initializing...\n");

    predictive_reset();
    INIT_DELAYED_WORK(&pd_work, predictive_work_fn);

    predictive_initialized = true;

    pr_info("smartmem: predictive initialized\n");
    return 0;
}

/**
 * 预测模型退出
 */
void predictive_exit(void)
{
    pr_info("smartmem: predictive exiting...\n");

    if (predictive_running)
        predictive_stop();

    predictive_initialized = false;

    pr_info("smartmem: predictive exited\n");
}

/**
 * 启动预测
 */
int predictive_start(void)
{
    pr_info("smartmem: starting predictive...\n");

    if (!predictive_initialized)
        return -ENODEV;

    predictive_running = true;

    /* 启动定期采样 */
    last_sample_time = ktime_get();
    schedule_delayed_work(&pd_work,
                          msecs_to_jiffies(PREDICT_SAMPLE_INTERVAL * 1000));

    pr_info("smartmem: predictive started (sample_interval=%ds)\n",
            PREDICT_SAMPLE_INTERVAL);
    return 0;
}

/**
 * 停止预测
 */
int predictive_stop(void)
{
    pr_info("smartmem: stopping predictive...\n");

    predictive_running = false;
    cancel_delayed_work_sync(&pd_work);

    pr_info("smartmem: predictive stopped (samples=%llu, predictions=%llu)\n",
            atomic64_read(&pd_stats.sample_count),
            atomic64_read(&pd_stats.prediction_count));
    return 0;
}
