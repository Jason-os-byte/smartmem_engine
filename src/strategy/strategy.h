/* 策略引擎 */

#ifndef _SMARTMEM_STRATEGY_H
#define _SMARTMEM_STRATEGY_H

#include "smartmem.h"

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

struct lru_strategy_ops {
	int (*page_accessed)(struct page *page);
	int (*page_referenced)(struct page *page);
	int (*page_evict)(struct page *page);
	int (*page_promote)(struct page *page);
	int (*page_demote)(struct page *page);
};

struct lru_strategy {
	char name[64];
	char version[16];
	char description[128];
	struct lru_strategy_ops ops;
	bool enabled;
	struct list_head list;
};

struct numa_strategy_ops {
	int (*check_imbalance)(void);
	int (*select_target_node)(int src_nid);
	int (*migrate_pages)(int src_nid, int dst_nid);
};

struct numa_strategy {
	char name[64];
	char version[16];
	char description[128];
	struct numa_strategy_ops ops;
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

int lru_strategy_register(struct lru_strategy *s);
int lru_strategy_unregister(struct lru_strategy *s);
struct lru_strategy *lru_strategy_get_current(void);
int lru_strategy_switch(const char *name);

int numa_strategy_register(struct numa_strategy *s);
int numa_strategy_unregister(struct numa_strategy *s);
struct numa_strategy *numa_strategy_get_current(void);
int numa_strategy_switch(const char *name);


#endif /* _SMARTMEM_STRATEGY_H */