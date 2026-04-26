/* 分析引擎 */
#ifndef _SMARTMEM_ANALYSIS_H
#define _SMARTMEM_ANALYSIS_H

#include "smartmem.h"

/* 前向声明 */
struct hotspot_top_entry;
struct bottleneck_entry;
struct root_cause_entry;

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

/* 查询接口 */
int smartmem_analysis_get_hotspots(struct hotspot_top_entry *entries, int n);
int smartmem_analysis_get_bottlenecks(struct bottleneck_entry *entries, int n);
int smartmem_analysis_get_root_causes(struct root_cause_entry *entries, int n);

#endif /* _SMARTMEM_ANALYSIS_H */