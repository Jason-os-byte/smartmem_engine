/* 预测模型 */

#ifndef _SMARTMEM_PREDICTIVE_H
#define _SMARTMEM_PREDICTIVE_H

#include "smartmem.h"

// 预测结果类型
enum prediction_type {
    PREDICT_NONE = 0,
    PREDICT_OOM_RISK, /* OOM 风险预测 */
    PREDICT_MEMORY_EXHAUST, /* 内存耗尽预测 */
    PREDICT_PRESSURE_INCREASE, /* 内存压力增加预测 */
    PREDICT_TYPE_MAX,
};

/* 预测严重级别 */
enum prediction_severity {
    PREDICT_SEV_LOW = 0,
    PREDICT_SEV_MEDIUM,
    PREDICT_SEV_HIGH,
};

struct prediction_result {
    enum prediction_type type;
    enum prediction_severity severity;
    u64 predicted_free_pct;   /* 预测的空闲率 */
    u64 estimated_time_sec;    /* 预计到达时间(秒)，0=已到达 */
    u64 confidence;            /* 置信度 0-100 */
    char description[128];
};

// 历史采样点
#define PREDICT_SAMPLE_COUNT 12 /* 保留最近12个采样 */
#define PREDICT_SAMPLE_INTERVAL 5 /* 每五秒采样一次 */

struct predict_sample {
    ktime_t timestamp;
    int free_pct;               /* 空闲内存百分比 */
    u64 alloc_rate;             /* 分配速率(页/秒) */
    u64 free_rate;              /* 释放速率(页/秒) */
};

/* 预测模型统计 */
struct predict_stats {
    atomic64_t sample_count;
    atomic64_t prediction_count;
    atomic64_t oom_predict_count;
};

/* 预测模型接口 */
int predictive_init(void);
void predictive_exit(void);
int predictive_start(void);
int predictive_stop(void);

/* 执行预测 */
int predictive_predict(struct prediction_result *result);

/* 获取统计 */
void predictive_get_stats(struct predict_stats *stats);

/* 重置 */
void predictive_reset(void);

#endif /* _SMARTMEM_PREDICTIVE_H */