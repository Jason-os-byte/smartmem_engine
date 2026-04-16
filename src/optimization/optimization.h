/* 优化引擎 */

#ifndef _SMARTMEM_OPTIMIZATION_H
#define _SMARTMEM_OPTIMIZATION_H

#include "smartmem.h"

/**
 * 优化引擎
 */
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

#endif /* _SMARTMEM_OPTIMIZATION_H */