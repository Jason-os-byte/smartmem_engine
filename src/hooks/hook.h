/* Hook 管理框架 */
#ifndef _SMARTMEM_HOOK_H
#define _SMARTMEM_HOOK_H

#include "smartmem.h"

/**
 * Hook类型
 */
enum smartmem_hook_type {
    SMARTMEM_HOOK_TYPE_BUDDY = 0,
    SMARTMEM_HOOK_TYPE_SLUB,
    SMARTMEM_HOOK_TYPE_VMA,
    SMARTMEM_HOOK_TYPE_LRU,
    SMARTMEM_HOOK_TYPE_NUMA,
    SMARTMEM_HOOK_TYPE_MAX,
};

/**
 * Hook操作函数
 */
struct smartmem_hook_ops {
    int (*init)(void);
    void (*exit)(void);
    int (*enable)(void);
    int (*disable)(void);
};

/**
 * Hook 描述
 */
struct smartmem_hook {
    char name[32];
    enum smartmem_hook_type type;
    struct smartmem_hook_ops ops;
    bool enabled;
    struct list_head list;
};

/* Hook 管理接口 */
int smartmem_hook_init(void);
void smartmem_hook_exit(void);
int smartmem_hook_register(struct smartmem_hook *hook);
int smartmem_hook_unregister(struct smartmem_hook *hook);
int smartmem_hook_enable(enum smartmem_hook_type type);
int smartmem_hook_disable(enum smartmem_hook_type type);


#endif /* _SMARTMEM_HOOK_H */