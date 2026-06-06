/* 配置系统实现 */

#include "config.h"
#include <linux/string.h>
#include <linux/slab.h>

/* 全局配置实例 */
static struct smartmem_config g_config = {
    .hook_buddy_enabled = false,
    .hook_slub_enabled = false,
    .hook_vma_enabled = false,
    .hook_lru_enabled = false,
    .hook_numa_enabled = false,
    .numa_aware_enabled = false,
    .adaptive_slub_enabled = false,
    .trace_enabled = false,
    .auto_tune_enabled = false,
    .initialized = false,
};

/* 配置锁 */
static DEFINE_SPINLOCK(config_lock);

/* 配置初始化 */
int smartmem_config_init(void)
{
    pr_info("smartmem: config initializing...\n");

    spin_lock_init(&config_lock);
    g_config.initialized = true;

    pr_info("smartmem: config initialized\n");
    return 0;
}

/* 配置退出 */
void smartmem_config_exit(void)
{
    pr_info("smartmem: config exiting...\n");

    spin_lock(&config_lock);
    g_config.initialized = false;
    spin_unlock(&config_lock);

    pr_info("smartmem: config exited\n");
}

/**
 * 设置布尔值配置
 */
static int config_set_bool(const char *key, bool value)
{
    int ret = 0;

    spin_lock(&config_lock);

    if (strcmp(key, "hook_buddy_enabled") == 0) {
        g_config.hook_buddy_enabled = value;
    } else if (strcmp(key, "hook_slub_enabled") == 0) {
        g_config.hook_slub_enabled = value;
    } else if (strcmp(key, "hook_vma_enabled") == 0) {
        g_config.hook_vma_enabled = value;
    } else if (strcmp(key, "hook_lru_enabled") == 0) {
        g_config.hook_lru_enabled = value;
    } else if (strcmp(key, "hook_numa_enabled") == 0) {
        g_config.hook_numa_enabled = value;
    } else if (strcmp(key, "numa_aware_enabled") == 0) {
        g_config.numa_aware_enabled = value;
    } else if (strcmp(key, "adaptive_slub_enabled") == 0) {
        g_config.adaptive_slub_enabled = value;
    } else if (strcmp(key, "trace_enabled") == 0) {
        g_config.trace_enabled = value;
    } else if (strcmp(key, "auto_tune_enabled") == 0) {
        g_config.auto_tune_enabled = value;
    } else {
        ret = -ENOENT;
    }
    
    spin_unlock(&config_lock);
    
    if (ret == 0) {
        pr_info("smartmem: config %s=%s\n", key, value ? "true" : "false");
    }
    
    return ret;
}

/**
 * 获取布尔值配置
 */
static int config_get_bool(const char *key, bool *value) 
{
    int ret = 0;

    spin_lock(&config_lock);

    if (strcmp(key, "hook_buddy_enabled") == 0) {
        *value = g_config.hook_buddy_enabled;
    } else if (strcmp(key, "hook_slub_enabled") == 0) {
        *value = g_config.hook_slub_enabled;
    } else if (strcmp(key, "hook_vma_enabled") == 0) {
        *value = g_config.hook_vma_enabled;
    } else if (strcmp(key, "hook_lru_enabled") == 0) {
        *value = g_config.hook_lru_enabled;
    } else if (strcmp(key, "hook_numa_enabled") == 0) {
        *value = g_config.hook_numa_enabled;
    } else if (strcmp(key, "numa_aware_enabled") == 0) {
        *value = g_config.numa_aware_enabled;
    } else if (strcmp(key, "adaptive_slub_enabled") == 0) {
        *value = g_config.adaptive_slub_enabled;
    } else if (strcmp(key, "trace_enabled") == 0) {
        *value = g_config.trace_enabled;
    } else if (strcmp(key, "auto_tune_enabled") == 0) {
        *value = g_config.auto_tune_enabled;
    } else {
        ret = -ENOENT;
    }
    
    spin_unlock(&config_lock);
    
    return ret;
}

/* 设置配置 */
int smartmem_config_set(const char *key, const char *value)
{
    bool bool_value;
    
    if (!key || !value) {
        pr_err("smartmem: invalid config key or value\n");
        return -EINVAL;
    }

    /* 解析布尔值 */
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
        bool_value = true;
    } else if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
        bool_value = false;
    } else {
        pr_err("smartmem: invalid config value '%s'\n", value);
        return -EINVAL;
    }

    return config_set_bool(key, bool_value);
}

/* 获取配置 */
int smartmem_config_get(const char *key, char *value, size_t len)
{
    bool bool_value;
    int ret;

    if (!key || !value) {
        pr_err("smartmem: invalid config key or buffer\n");
        return -EINVAL;
    }

    ret = config_get_bool(key, &bool_value);
    if (ret) {
        return ret;
    }
    
    snprintf(value, len, "%s", bool_value ? "true" : "false");
    return 0;
}
