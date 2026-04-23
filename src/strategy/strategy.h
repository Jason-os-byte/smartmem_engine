/* 策略引擎 */

#ifndef _SMARTMEM_STRATEGY_H
#define _SMARTMEM_STRATEGY_H

#include "smartmem.h"
#include "numa_buddy.h"
#include "adaptive_slub.h"

/**
 * Buddy 策略操作
 */
struct buddy_strategy_ops {
    int (*pre_alloc)(int order, u32 *gfp_mask, int *nid);
    int (*post_alloc)(void *page, int order, u32 gfp_mask);
    int (*pre_free)(void *page, int order);
    int (*post_free)(int order);
    int (*select_node)(int preferred_nid, u32 gfp_mask);
};

/**
 * Buddy 策略
 */
struct buddy_strategy {
    char name[64];
    char version[16];
    char description[128];
    struct buddy_strategy_ops ops;
    bool enabled;
    struct list_head list;
};

/**
 * SLUB 策略操作
 */
struct slub_strategy_ops {
    int (*pre_alloc)(size_t size, u32 *gfp_mask);
    int (*post_alloc)(void *ptr, size_t size);
    int (*pre_free)(void *ptr);
    int (*post_free)(void);
};

struct slub_strategy {
    char name[64];
    char version[16];
    char description[128];
    struct slub_strategy_ops ops;
    bool enabled;
    struct list_head list;
};

/**
 * 策略引擎接口
 */
int smartmem_strategy_init(void);
void smartmem_strategy_exit(void);
int buddy_strategy_register(struct buddy_strategy *s);
int buddy_strategy_unregister(struct buddy_strategy *s);
struct buddy_strategy *buddy_strategy_get_current(void);
int buddy_strategy_switch(const char *name);

int slub_strategy_register(struct slub_strategy *s);
int slub_strategy_unregister(struct slub_strategy *s);
struct slub_strategy *slub_strategy_get_current(void);
int slub_strategy_switch(const char *name);

#endif /* _SMARTMEM_STRATEGY_H */