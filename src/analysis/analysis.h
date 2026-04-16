/* 分析引擎 */
#ifndef _SMARTMEM_ANALYSIS_H
#define _SMARTMEM_ANALYSIS_H

#include "smartmem.h"

/**
 * 分析引擎
 */
struct smartmem_analysis {
    bool hotspot_enabled;
    bool bottleneck_enabled;
    bool root_cause_enabled;
    bool initialized;
};

/* 分析接口 */
int smartmem_analysis_init(void);
void smartmem_analysis_exit(void);
int smartmem_analysis_hotspot_update(void);
int smartmem_analysis_bottleneck_update(void);
int smartmem_analysis_root_cause_update(void);

#endif /* _SMARTMEM_ANALYSIS_H */