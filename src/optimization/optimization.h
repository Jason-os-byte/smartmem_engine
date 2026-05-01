/* 优化引擎 */
#ifndef _SMARTMEM_OPTIMIZATION_H
#define _SMARTMEM_OPTIMIZATION_H

#include "smartmem.h"

struct smartmem_optimization {
    bool auto_tune_enabled;
    bool predictive_enabled;
    bool initialized;
};

/* 优化接口 */
int smartmem_optimization_init(void);
void smartmem_optimization_exit(void);
int smartmem_optimization_start(void);
int smartmem_optimization_stop(void);

/* 调优接口 */
struct tune_stats;
struct tune_history;
int smartmem_optimization_tune_check(void);
int smartmem_optimization_tune_trigger(int action);
void smartmem_optimization_get_tune_stats(struct tune_stats *stats);
int smartmem_optimization_get_tune_history(struct tune_history *entries, int max);

/* 预测接口 */
struct prediction_result;
int smartmem_optimization_predict(struct prediction_result *result);

#endif /* _SMARTMEM_OPTIMIZATION_H */
