/* 配置系统 */
#ifndef _SMARTMEM_CONFIG_H
#define _SMARTMEM_CONFIG_H

#include "smartmem.h"

/* 配置结构 */
struct smartmem_config {
    /* Hook配置 */
    bool hook_buddy_enabled;
    bool hook_slub_enabled;
    bool hook_vma_enabled;
    bool hook_lru_enabled;
    bool hook_numa_enabled;

    /* 策略配置 */
    bool numa_aware_enabled;
    bool adaptive_slub_enabled;

    /* 监控配置 */
    bool ebpf_enabled;
    bool trace_enabled;

    /* 优化配置 */
    bool auto_tune_enabled;

    /* 模块状态 */
    bool initialized;
};

/* 配置接口 */
int smartmem_config_init(void);
void smartmem_config_exit(void);
int smartmem_config_set(const char *key, const char *value);
int smartmem_config_get(const char *key, char *value, size_t len);

#endif /* _SMARTMEM_CONFIG_H */